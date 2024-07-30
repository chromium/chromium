// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_security_key_handler.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chrome/browser/webauthn/cablev2_devices.h"
#include "chrome/browser/webauthn/local_credential_management.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/credential_management.h"
#include "device/fido/fido_constants.h"
#include "device/fido/pin.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "device/fido/reset_request_handler.h"
#include "device/fido/set_pin_request_handler.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_WIN)
#include "device/fido/win/webauthn_api.h"
#endif

using content::BrowserThread;

namespace {

base::flat_set<device::FidoTransportProtocol> supported_transports() {
  // If we ever support BLE devices then additional thought will be required
  // in the UI; therefore don't enable them here. NFC is not supported on
  // desktop thus only USB devices remain to be enabled.
  return {device::FidoTransportProtocol::kUsbHumanInterfaceDevice};
}

void HandleClose(base::RepeatingClosure close_callback,
                 const base::Value::List& args) {
  DCHECK_EQ(0u, args.size());
  close_callback.Run();
}

base::Value::Dict EncodeEnrollment(const std::vector<uint8_t>& id,
                                   const std::string& name) {
  base::Value::Dict value;
  value.Set("name", name);
  value.Set("id", base::HexEncode(id));
  return value;
}

bool DecodePublicKey(const std::string& value,
                     std::array<uint8_t, device::kP256X962Length>* out) {
  std::string bytes;
  if (!base::Base64Decode(value, &bytes) || bytes.size() != out->size()) {
    return false;
  }

  base::ranges::copy(bytes, out->begin());
  return true;
}

}  // namespace

namespace settings {

SecurityKeysHandlerBase::SecurityKeysHandlerBase() = default;
SecurityKeysHandlerBase::SecurityKeysHandlerBase(
    std::unique_ptr<device::FidoDiscoveryFactory> discovery_factory)
    : discovery_factory_(std::move(discovery_factory)) {}
SecurityKeysHandlerBase::~SecurityKeysHandlerBase() = default;

void SecurityKeysHandlerBase::OnJavascriptAllowed() {}

void SecurityKeysHandlerBase::OnJavascriptDisallowed() {
  // If Javascript is disallowed, |Close| will invalidate all current WeakPtrs
  // and thus drop all pending callbacks. This means that
  // |IsJavascriptAllowed| doesn't need to be tested before each callback
  // because, if the callback into this object happened, then Javascript is
  // allowed.
  Close();
}

SecurityKeysPINHandler::SecurityKeysPINHandler() = default;
SecurityKeysPINHandler::~SecurityKeysPINHandler() = default;

void SecurityKeysPINHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "securityKeyStartSetPIN",
      base::BindRepeating(&SecurityKeysPINHandler::HandleStartSetPIN,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "securityKeySetPIN",
      base::BindRepeating(&SecurityKeysPINHandler::HandleSetPIN,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "securityKeyPINClose",
      base::BindRepeating(&HandleClose,
                          base::BindRepeating(&SecurityKeysPINHandler::Close,
                                              base::Unretained(this))));
}

void SecurityKeysPINHandler::Close() {
  // Invalidate all existing WeakPtrs so that no stale callbacks occur.
  weak_factory_.InvalidateWeakPtrs();
  state_ = State::kNone;
  set_pin_.reset();
  callback_id_.clear();
}

void SecurityKeysPINHandler::HandleStartSetPIN(const base::Value::List& args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(State::kNone, state_);
  DCHECK_EQ(1u, args.size());

  AllowJavascript();
  DCHECK(callback_id_.empty());
  callback_id_ = args[0].GetString();
  state_ = State::kStartSetPIN;
  set_pin_ = std::make_unique<device::SetPINRequestHandler>(
      supported_transports(),
      base::BindOnce(&SecurityKeysPINHandler::OnGatherPIN,
                     weak_factory_.GetWeakPtr()),
      base::BindRepeating(&SecurityKeysPINHandler::OnSetPINComplete,
                          weak_factory_.GetWeakPtr()));
}

void SecurityKeysPINHandler::OnGatherPIN(uint32_t current_min_pin_length,
                                         uint32_t new_min_pin_length,
                                         std::optional<int64_t> num_retries) {
  DCHECK_EQ(State::kStartSetPIN, state_);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::Value::Dict response;
  response.Set("done", false);
  response.Set("error", base::Value());
  response.Set("currentMinPinLength", static_cast<int>(current_min_pin_length));
  response.Set("newMinPinLength", static_cast<int>(new_min_pin_length));
  if (num_retries) {
    state_ = State::kGatherChangePIN;
    response.Set("retries", static_cast<int>(*num_retries));
  } else {
    state_ = State::kGatherNewPIN;
    response.Set("retries", base::Value());
  }

  ResolveJavascriptCallback(base::Value(std::move(callback_id_)), response);
}

void SecurityKeysPINHandler::OnSetPINComplete(
    device::CtapDeviceResponseCode code) {
  DCHECK(state_ == State::kStartSetPIN || state_ == State::kSettingPIN);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (code == device::CtapDeviceResponseCode::kCtap2ErrPinInvalid) {
    // In the event that the old PIN was incorrect, the UI may prompt again.
    state_ = State::kGatherChangePIN;
  } else {
    state_ = State::kNone;
    set_pin_.reset();
  }

  base::Value::Dict response;
  response.Set("done", true);
  response.Set("error", static_cast<int>(code));
  ResolveJavascriptCallback(base::Value(std::move(callback_id_)), response);
}

void SecurityKeysPINHandler::HandleSetPIN(const base::Value::List& args) {
  DCHECK(state_ == State::kGatherNewPIN || state_ == State::kGatherChangePIN);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(3u, args.size());

  DCHECK(callback_id_.empty());
  callback_id_ = args[0].GetString();
  const std::string old_pin = args[1].GetString();
  const std::string new_pin = args[2].GetString();

  DCHECK((state_ == State::kGatherNewPIN) == old_pin.empty());

  CHECK_EQ(device::pin::ValidatePIN(new_pin),
           device::pin::PINEntryError::kNoError);
  state_ = State::kSettingPIN;
  set_pin_->ProvidePIN(old_pin, new_pin);
}

SecurityKeysResetHandler::SecurityKeysResetHandler() = default;
SecurityKeysResetHandler::~SecurityKeysResetHandler() = default;

void SecurityKeysResetHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "securityKeyReset",
      base::BindRepeating(&SecurityKeysResetHandler::HandleReset,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "securityKeyCompleteReset",
      base::BindRepeating(&SecurityKeysResetHandler::HandleCompleteReset,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "securityKeyResetClose",
      base::BindRepeating(&HandleClose,
                          base::BindRepeating(&SecurityKeysResetHandler::Close,
                                              base::Unretained(this))));
}

void SecurityKeysResetHandler::Close() {
  // Invalidate all existing WeakPtrs so that no stale callbacks occur.
  weak_factory_.InvalidateWeakPtrs();
  state_ = State::kNone;
  reset_.reset();
  callback_id_.clear();
}

void SecurityKeysResetHandler::HandleReset(const base::Value::List& args) {
  DCHECK_EQ(State::kNone, state_);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(1u, args.size());

  AllowJavascript();
  DCHECK(callback_id_.empty());
  callback_id_ = args[0].GetString();

  state_ = State::kStartReset;
  reset_ = std::make_unique<device::ResetRequestHandler>(
      supported_transports(),
      base::BindOnce(&SecurityKeysResetHandler::OnResetSent,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&SecurityKeysResetHandler::OnResetFinished,
                     weak_factory_.GetWeakPtr()));
}

void SecurityKeysResetHandler::OnResetSent() {
  DCHECK_EQ(State::kStartReset, state_);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // A reset message has been sent to a security key and it may complete
  // before Javascript asks for the result. Therefore |HandleCompleteReset|
  // and |OnResetFinished| may be called in either order.
  state_ = State::kWaitingForResetNoCallbackYet;
  ResolveJavascriptCallback(base::Value(std::move(callback_id_)),
                            base::Value(0 /* success */));
}

void SecurityKeysResetHandler::HandleCompleteReset(
    const base::Value::List& args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(1u, args.size());

  DCHECK(callback_id_.empty());
  callback_id_ = args[0].GetString();

  switch (state_) {
    case State::kWaitingForResetNoCallbackYet:
      // The reset operation hasn't completed. |callback_id_| will be used in
      // |OnResetFinished| when it does.
      state_ = State::kWaitingForResetHaveCallback;
      break;

    case State::kWaitingForCompleteReset:
      // The reset operation has completed and we were waiting for this
      // call from Javascript in order to provide the result.
      state_ = State::kNone;
      ResolveJavascriptCallback(base::Value(std::move(callback_id_)),
                                base::Value(static_cast<int>(*reset_result_)));
      reset_.reset();
      break;

    default:
      NOTREACHED_IN_MIGRATION();
  }
}

void SecurityKeysResetHandler::OnResetFinished(
    device::CtapDeviceResponseCode result) {
  switch (state_) {
    case State::kWaitingForResetNoCallbackYet:
      // The reset operation has completed, but Javascript hasn't called
      // |CompleteReset| so we cannot make the callback yet.
      state_ = State::kWaitingForCompleteReset;
      reset_result_ = result;
      break;

    case State::kStartReset:
      // The reset operation failed immediately, probably because the user
      // selected a U2F device. |callback_id_| has been set by |Reset|.
      [[fallthrough]];

    case State::kWaitingForResetHaveCallback:
      // The |CompleteReset| call has already provided |callback_id_| so the
      // reset can be completed immediately.
      state_ = State::kNone;
      ResolveJavascriptCallback(base::Value(std::move(callback_id_)),
                                base::Value(static_cast<int>(result)));
      reset_.reset();
      break;

    default:
      NOTREACHED_IN_MIGRATION();
  }
}

SecurityKeysCredentialHandler::SecurityKeysCredentialHandler() = default;
SecurityKeysCredentialHandler::SecurityKeysCredentialHandler(
    std::unique_ptr<device::FidoDiscoveryFactory> discovery_factory)
    : SecurityKeysHandlerBase(std::move(discovery_factory)) {}
SecurityKeysCredentialHandler::~SecurityKeysCredentialHandler() = default;

void SecurityKeysCredentialHandler::HandleStart(const base::Value::List& args) {
  DCHECK_EQ(State::kNone, state_);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(1u, args.size());
  DCHECK(!credential_management_);

  AllowJavascript();
  DCHECK(callback_id_.empty());
  callback_id_ = args[0].GetString();

  state_ = State::kStart;
  credential_management_ =
      std::make_unique<device::CredentialManagementHandler>(
          discovery_factory(), supported_transports(),
          base::BindOnce(
              &SecurityKeysCredentialHandler::OnCredentialManagementReady,
              weak_factory_.GetWeakPtr()),
          base::BindRepeating(&SecurityKeysCredentialHandler::OnGatherPIN,
                              weak_factory_.GetWeakPtr()),
          base::BindOnce(&SecurityKeysCredentialHandler::OnFinished,
                         weak_factory_.GetWeakPtr()));
}

void SecurityKeysCredentialHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "securityKeyCredentialManagementStart",
      base::BindRepeating(&SecurityKeysCredentialHandler::HandleStart,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "securityKeyCredentialManagementPIN",
      base::BindRepeating(&SecurityKeysCredentialHandler::HandlePIN,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "securityKeyCredentialManagementEnumerate",
      base::BindRepeating(&SecurityKeysCredentialHandler::HandleEnumerate,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "securityKeyCredentialManagementDelete",
      base::BindRepeating(&SecurityKeysCredentialHandler::HandleDelete,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "securityKeyCredentialManagementUpdate",
      base::BindRepeating(
          &SecurityKeysCredentialHandler::HandleUpdateUserInformation,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "securityKeyCredentialManagementClose",
      base::BindRepeating(
          &HandleClose,
          base::BindRepeating(&SecurityKeysCredentialHandler::Close,
                              base::Unretained(this))));
}

void SecurityKeysCredentialHandler::Close() {
  // Invalidate all existing WeakPtrs so that no stale callbacks occur.
  weak_factory_.InvalidateWeakPtrs();
  state_ = State::kNone;
  credential_management_.reset();
  callback_id_.clear();
  credential_management_provide_pin_cb_.Reset();
  DCHECK(!credential_management_provide_pin_cb_);
}

void SecurityKeysCredentialHandler::HandlePIN(const base::Value::List& args) {
  DCHECK_EQ(State::kPIN, state_);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(2u, args.size());
  DCHECK(credential_management_);
  DCHECK(credential_management_provide_pin_cb_);
  DCHECK(callback_id_.empty());

  callback_id_ = args[0].GetString();
  std::string pin = args[1].GetString();

  std::move(credential_management_provide_pin_cb_).Run(pin);
}

void SecurityKeysCredentialHandler::HandleEnumerate(
    const base::Value::List& args) {
  DCHECK_EQ(state_, State::kReady);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(1u, args.size());
  DCHECK(credential_management_);
  DCHECK(callback_id_.empty());

  state_ = State::kGettingCredentials;
  callback_id_ = args[0].GetString();
  credential_management_->GetCredentials(
      base::BindOnce(&SecurityKeysCredentialHandler::OnHaveCredentials,
                     weak_factory_.GetWeakPtr()));
}

void SecurityKeysCredentialHandler::HandleDelete(
    const base::Value::List& args) {
  DCHECK_EQ(State::kReady, state_);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(2u, args.size());
  DCHECK(credential_management_);
  DCHECK(callback_id_.empty());

  state_ = State::kDeletingCredentials;
  callback_id_ = args[0].GetString();
  std::vector<device::PublicKeyCredentialDescriptor> credential_ids;
  for (const base::Value& el : args[1].GetList()) {
    std::vector<uint8_t> credential_id_bytes;
    if (!base::HexStringToBytes(el.GetString(), &credential_id_bytes)) {
      NOTREACHED_IN_MIGRATION();
      continue;
    }
    device::PublicKeyCredentialDescriptor credential_id(
        device::CredentialType::kPublicKey, std::move(credential_id_bytes));
    credential_ids.emplace_back(std::move(credential_id));
  }
  credential_management_->DeleteCredentials(
      std::move(credential_ids),
      base::BindOnce(&SecurityKeysCredentialHandler::OnCredentialsDeleted,
                     weak_factory_.GetWeakPtr()));
}

void SecurityKeysCredentialHandler::HandleUpdateUserInformation(
    const base::Value::List& args) {
  DCHECK_EQ(State::kReady, state_);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(5u, args.size());
  DCHECK(credential_management_);
  DCHECK(callback_id_.empty());

  state_ = State::kUpdatingUserInformation;
  callback_id_ = args[0].GetString();

  std::vector<uint8_t> credential_id_bytes;
  if (!base::HexStringToBytes(args[1].GetString(), &credential_id_bytes)) {
    NOTREACHED_IN_MIGRATION();
  }
  device::PublicKeyCredentialDescriptor credential_id(
      device::CredentialType::kPublicKey, credential_id_bytes);

  std::vector<uint8_t> user_handle;
  if (!base::HexStringToBytes(args[2].GetString(), &user_handle)) {
    NOTREACHED_IN_MIGRATION();
  }
  std::string new_username = args[3].GetString();
  std::string new_displayname = args[4].GetString();

  device::PublicKeyCredentialUserEntity updated_user(
      std::move(user_handle), std::move(new_username),
      std::move(new_displayname));

  credential_management_->UpdateUserInformation(
      std::move(credential_id), std::move(updated_user),
      base::BindOnce(&SecurityKeysCredentialHandler::OnUserInformationUpdated,
                     weak_factory_.GetWeakPtr()));
}

void SecurityKeysCredentialHandler::OnCredentialManagementReady() {
  DCHECK(state_ == State::kStart || state_ == State::kPIN);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(credential_management_);
  DCHECK(!callback_id_.empty());

  state_ = State::kReady;
  ResolveJavascriptCallback(base::Value(std::move(callback_id_)),
                            base::Value());
}

void SecurityKeysCredentialHandler::OnHaveCredentials(
    device::CtapDeviceResponseCode status,
    std::optional<std::vector<device::AggregatedEnumerateCredentialsResponse>>
        responses,
    std::optional<size_t> remaining_credentials) {
  DCHECK_EQ(State::kGettingCredentials, state_);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(credential_management_);
  DCHECK(!callback_id_.empty());

  if (status == device::CtapDeviceResponseCode::kCtap2ErrNoCredentials) {
    OnFinished(device::CredentialManagementStatus::kNoCredentials);
    return;
  }
  if (status != device::CtapDeviceResponseCode::kSuccess) {
    OnFinished(
        device::CredentialManagementStatus::kAuthenticatorResponseInvalid);
    return;
  }
  DCHECK(responses);
  DCHECK(remaining_credentials);

  state_ = State::kReady;

  base::Value::List credentials;
  for (const auto& response : *responses) {
    for (const auto& credential : response.credentials) {
      base::Value::Dict credential_dict;
      std::string credential_id = base::HexEncode(credential.credential_id.id);
      if (credential_id.empty()) {
        NOTREACHED_IN_MIGRATION();
        continue;
      }
      std::string userHandle = base::HexEncode(credential.user.id);

      credential_dict.Set("credentialId", std::move(credential_id));
      credential_dict.Set("relyingPartyId", response.rp.id);
      credential_dict.Set("userHandle", std::move(userHandle));
      credential_dict.Set("userName", credential.user.name.value_or(""));
      credential_dict.Set("userDisplayName",
                          credential.user.display_name.value_or(""));
      credentials.Append(std::move(credential_dict));
    }
  }

  ResolveJavascriptCallback(base::Value(std::move(callback_id_)), credentials);
}

void SecurityKeysCredentialHandler::OnGatherPIN(
    device::CredentialManagementHandler::AuthenticatorProperties
        authenticator_properties,
    base::OnceCallback<void(std::string)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback_id_.empty());
  DCHECK(!credential_management_provide_pin_cb_);

  credential_management_provide_pin_cb_ = std::move(callback);
  if (state_ == State::kStart) {
    // Resolve the promise to startCredentialManagement().
    base::Value::Dict response;
    response.Set("minPinLength",
                 static_cast<int>(authenticator_properties.min_pin_length));
    response.Set("supportsUpdateUserInformation",
                 authenticator_properties.supports_update_user_information);
    state_ = State::kPIN;
    ResolveJavascriptCallback(base::Value(std::move(callback_id_)), response);
    return;
  }

  // Resolve the promise to credentialManagementProvidePIN().
  DCHECK_EQ(state_, State::kPIN);
  base::Value::List response;
  response.Append(static_cast<int>(authenticator_properties.min_pin_length));
  response.Append(static_cast<int>(authenticator_properties.pin_retries));
  ResolveJavascriptCallback(base::Value(std::move(callback_id_)), response);
}

void SecurityKeysCredentialHandler::OnCredentialsDeleted(
    device::CtapDeviceResponseCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(State::kDeletingCredentials, state_);
  DCHECK(credential_management_);
  DCHECK(!callback_id_.empty());

  state_ = State::kReady;

  base::Value::Dict response;
  response.Set("success", status == device::CtapDeviceResponseCode::kSuccess);
  response.Set(
      "message",
      l10n_util::GetStringUTF8(
          status == device::CtapDeviceResponseCode::kSuccess
              ? IDS_SETTINGS_SECURITY_KEYS_CREDENTIAL_MANAGEMENT_DELETE_SUCCESS
              : IDS_SETTINGS_SECURITY_KEYS_CREDENTIAL_MANAGEMENT_DELETE_FAILED));
  ResolveJavascriptCallback(base::Value(std::move(callback_id_)), response);
}

void SecurityKeysCredentialHandler::OnUserInformationUpdated(
    device::CtapDeviceResponseCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(State::kUpdatingUserInformation, state_);
  DCHECK(credential_management_);
  DCHECK(!callback_id_.empty());

  state_ = State::kReady;

  base::Value::Dict response;
  response.Set("success", status == device::CtapDeviceResponseCode::kSuccess);
  response.Set(
      "message",
      l10n_util::GetStringUTF8(
          status == device::CtapDeviceResponseCode::kSuccess
              ? IDS_SETTINGS_SECURITY_KEYS_CREDENTIAL_MANAGEMENT_UPDATE_SUCCESS
              : IDS_SETTINGS_SECURITY_KEYS_CREDENTIAL_MANAGEMENT_UPDATE_FAILED));
  ResolveJavascriptCallback(base::Value(std::move(callback_id_)), response);
}

void SecurityKeysCredentialHandler::OnFinished(
    device::CredentialManagementStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  int error;
  bool requires_pin_change = false;
  switch (status) {
    case device::CredentialManagementStatus::kSoftPINBlock:
      error = IDS_SETTINGS_SECURITY_KEYS_PIN_SOFT_LOCK;
      break;
    case device::CredentialManagementStatus::kHardPINBlock:
      error = IDS_SETTINGS_SECURITY_KEYS_PIN_HARD_LOCK;
      break;
    case device::CredentialManagementStatus::kNoCredentials:
      error = IDS_SETTINGS_SECURITY_KEYS_CREDENTIAL_MANAGEMENT_NO_CREDENTIALS;
      break;
    case device::CredentialManagementStatus::
        kAuthenticatorMissingCredentialManagement:
      error = IDS_SETTINGS_SECURITY_KEYS_NO_CREDENTIAL_MANAGEMENT;
      break;
    case device::CredentialManagementStatus::kNoPINSet:
      requires_pin_change = true;
      error = IDS_SETTINGS_SECURITY_KEYS_CREDENTIAL_MANAGEMENT_NO_PIN;
      break;
    case device::CredentialManagementStatus::kAuthenticatorResponseInvalid:
      error = IDS_SETTINGS_SECURITY_KEYS_CREDENTIAL_MANAGEMENT_ERROR;
      break;
    case device::CredentialManagementStatus::kForcePINChange:
      requires_pin_change = true;
      error = IDS_SETTINGS_SECURITY_KEYS_FORCE_PIN_CHANGE;
      break;
    case device::CredentialManagementStatus::kSuccess:
      error = IDS_SETTINGS_SECURITY_KEYS_CREDENTIAL_MANAGEMENT_REMOVED;
      break;
  }

  FireWebUIListener("security-keys-credential-management-finished",
                    base::Value(l10n_util::GetStringUTF8(error)),
                    base::Value(requires_pin_change));
}

SecurityKeysBioEnrollmentHandler::SecurityKeysBioEnrollmentHandler() = default;
SecurityKeysBioEnrollmentHandler::SecurityKeysBioEnrollmentHandler(
    std::unique_ptr<device::FidoDiscoveryFactory> discovery_factory)
    : SecurityKeysHandlerBase(std::move(discovery_factory)) {}
SecurityKeysBioEnrollmentHandler::~SecurityKeysBioEnrollmentHandler() = default;

void SecurityKeysBioEnrollmentHandler::HandleStart(
    const base::Value::List& args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(state_, State::kNone);
  DCHECK_EQ(1u, args.size());
  DCHECK(callback_id_.empty());

  AllowJavascript();
  state_ = State::kStart;
  callback_id_ = args[0].GetString();
  bio_ = std::make_unique<device::BioEnrollmentHandler>(
      supported_transports(),
      base::BindOnce(&SecurityKeysBioEnrollmentHandler::OnReady,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&SecurityKeysBioEnrollmentHandler::OnError,
                     weak_factory_.GetWeakPtr()),
      base::BindRepeating(&SecurityKeysBioEnrollmentHandler::OnGatherPIN,
                          weak_factory_.GetWeakPtr()),
      discovery_factory());
}

void SecurityKeysBioEnrollmentHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "securityKeyBioEnrollStart",
      base::BindRepeating(&SecurityKeysBioEnrollmentHandler::HandleStart,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "securityKeyBioEnrollProvidePIN",
      base::BindRepeating(&SecurityKeysBioEnrollmentHandler::HandleProvidePIN,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "securityKeyBioEnrollGetSensorInfo",
      base::BindRepeating(
          &SecurityKeysBioEnrollmentHandler::HandleGetSensorInfo,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "securityKeyBioEnrollEnumerate",
      base::BindRepeating(&SecurityKeysBioEnrollmentHandler::HandleEnumerate,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "securityKeyBioEnrollStartEnrolling",
      base::BindRepeating(
          &SecurityKeysBioEnrollmentHandler::HandleStartEnrolling,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "securityKeyBioEnrollDelete",
      base::BindRepeating(&SecurityKeysBioEnrollmentHandler::HandleDelete,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "securityKeyBioEnrollRename",
      base::BindRepeating(&SecurityKeysBioEnrollmentHandler::HandleRename,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "securityKeyBioEnrollCancel",
      base::BindRepeating(&SecurityKeysBioEnrollmentHandler::HandleCancel,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "securityKeyBioEnrollClose",
      base::BindRepeating(
          &HandleClose,
          base::BindRepeating(&SecurityKeysBioEnrollmentHandler::Close,
                              base::Unretained(this))));
}

void SecurityKeysBioEnrollmentHandler::Close() {
  weak_factory_.InvalidateWeakPtrs();
  state_ = State::kNone;
  bio_.reset();
  callback_id_.clear();
  provide_pin_cb_.Reset();
}

void SecurityKeysBioEnrollmentHandler::OnReady(
    device::BioEnrollmentHandler::SensorInfo sensor_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(bio_);
  DCHECK_EQ(state_, State::kGatherPIN);
  DCHECK(!callback_id_.empty());
  state_ = State::kReady;
  sensor_info_ = std::move(sensor_info);
  ResolveJavascriptCallback(base::Value(std::move(callback_id_)),
                            base::Value());
}

void SecurityKeysBioEnrollmentHandler::OnError(
    device::BioEnrollmentHandler::Error error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  state_ = State::kNone;

  int error_message;
  bool requires_pin_change = false;
  using Error = device::BioEnrollmentHandler::Error;
  switch (error) {
    case Error::kAuthenticatorRemoved:
      error_message = IDS_SETTINGS_SECURITY_KEYS_CREDENTIAL_MANAGEMENT_REMOVED;
      break;
    case Error::kSoftPINBlock:
      error_message = IDS_SETTINGS_SECURITY_KEYS_PIN_SOFT_LOCK;
      break;
    case Error::kHardPINBlock:
      error_message = IDS_SETTINGS_SECURITY_KEYS_PIN_HARD_LOCK;
      break;
    case Error::kAuthenticatorMissingBioEnrollment:
      error_message = IDS_SETTINGS_SECURITY_KEYS_NO_BIOMETRIC_ENROLLMENT;
      break;
    case Error::kNoPINSet:
      requires_pin_change = true;
      error_message = IDS_SETTINGS_SECURITY_KEYS_BIO_NO_PIN;
      break;
    case Error::kAuthenticatorResponseInvalid:
      error_message = IDS_SETTINGS_SECURITY_KEYS_CREDENTIAL_MANAGEMENT_ERROR;
      break;
    case Error::kForcePINChange:
      requires_pin_change = true;
      error_message = IDS_SETTINGS_SECURITY_KEYS_FORCE_PIN_CHANGE;
      break;
  }

  FireWebUIListener("security-keys-bio-enroll-error",
                    base::Value(l10n_util::GetStringUTF8(error_message)),
                    base::Value(requires_pin_change));

  // If |callback_id_| is not empty, there is an ongoing operation,
  // which means there is an unresolved Promise. Reject it so that
  // it isn't leaked.
  if (!callback_id_.empty()) {
    RejectJavascriptCallback(base::Value(std::move(callback_id_)),
                             base::Value());
  }
}

void SecurityKeysBioEnrollmentHandler::OnGatherPIN(
    uint32_t min_pin_length,
    int64_t retries,
    base::OnceCallback<void(std::string)> cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback_id_.empty());
  DCHECK(state_ == State::kStart || state_ == State::kGatherPIN);
  state_ = State::kGatherPIN;
  provide_pin_cb_ = std::move(cb);
  base::Value::List response;
  response.Append(static_cast<int>(min_pin_length));
  response.Append(static_cast<int>(retries));
  ResolveJavascriptCallback(base::Value(std::move(callback_id_)), response);
}

void SecurityKeysBioEnrollmentHandler::HandleProvidePIN(
    const base::Value::List& args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(2u, args.size());
  DCHECK_EQ(state_, State::kGatherPIN);
  state_ = State::kGatherPIN;
  callback_id_ = args[0].GetString();
  std::move(provide_pin_cb_).Run(args[1].GetString());
}

void SecurityKeysBioEnrollmentHandler::HandleGetSensorInfo(
    const base::Value::List& args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(1u, args.size());
  DCHECK_EQ(state_, State::kReady);
  base::Value::Dict response;
  response.Set("maxTemplateFriendlyName",
               static_cast<int>(sensor_info_.max_template_friendly_name));
  if (sensor_info_.max_samples_for_enroll) {
    response.Set("maxSamplesForEnroll", *sensor_info_.max_samples_for_enroll);
  }
  ResolveJavascriptCallback(base::Value(args[0].GetString()), response);
}

void SecurityKeysBioEnrollmentHandler::HandleEnumerate(
    const base::Value::List& args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(1u, args.size());
  DCHECK_EQ(state_, State::kReady);
  state_ = State::kEnumerating;
  callback_id_ = args[0].GetString();
  bio_->EnumerateTemplates(
      base::BindOnce(&SecurityKeysBioEnrollmentHandler::OnHaveEnumeration,
                     weak_factory_.GetWeakPtr()));
}

void SecurityKeysBioEnrollmentHandler::OnHaveEnumeration(
    device::CtapDeviceResponseCode code,
    std::optional<std::map<std::vector<uint8_t>, std::string>> enrollments) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback_id_.empty());
  DCHECK_EQ(state_, State::kEnumerating);

  base::Value::List list;
  if (enrollments) {
    for (const auto& enrollment : *enrollments) {
      list.Append(EncodeEnrollment(enrollment.first, enrollment.second));
    }
  }

  state_ = State::kReady;
  ResolveJavascriptCallback(base::Value(std::move(callback_id_)), list);
}

void SecurityKeysBioEnrollmentHandler::HandleStartEnrolling(
    const base::Value::List& args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(1u, args.size());
  DCHECK_EQ(state_, State::kReady);
  state_ = State::kEnrolling;
  callback_id_ = args[0].GetString();
  bio_->EnrollTemplate(
      base::BindRepeating(
          &SecurityKeysBioEnrollmentHandler::OnEnrollingResponse,
          weak_factory_.GetWeakPtr()),
      base::BindOnce(&SecurityKeysBioEnrollmentHandler::OnEnrollmentFinished,
                     weak_factory_.GetWeakPtr()));
}

void SecurityKeysBioEnrollmentHandler::OnEnrollingResponse(
    device::BioEnrollmentSampleStatus status,
    uint8_t remaining_samples) {
  DCHECK_EQ(state_, State::kEnrolling);
  base::Value::Dict d;
  d.Set("status", static_cast<int>(status));
  d.Set("remaining", static_cast<int>(remaining_samples));
  FireWebUIListener("security-keys-bio-enroll-status", d);
}

void SecurityKeysBioEnrollmentHandler::OnEnrollmentFinished(
    device::CtapDeviceResponseCode code,
    std::vector<uint8_t> template_id) {
  DCHECK_EQ(state_, State::kEnrolling);
  DCHECK(!callback_id_.empty());
  if (code == device::CtapDeviceResponseCode::kCtap2ErrKeepAliveCancel ||
      code == device::CtapDeviceResponseCode::kCtap2ErrFpDatabaseFull) {
    state_ = State::kReady;
    base::Value::Dict d;
    d.Set("code", static_cast<int>(code));
    d.Set("remaining", 0);
    ResolveJavascriptCallback(base::Value(std::move(callback_id_)), d);
    return;
  }
  if (code != device::CtapDeviceResponseCode::kSuccess) {
    OnError(device::BioEnrollmentHandler::Error::kAuthenticatorResponseInvalid);
    return;
  }
  bio_->EnumerateTemplates(base::BindOnce(
      &SecurityKeysBioEnrollmentHandler::OnHavePostEnrollmentEnumeration,
      weak_factory_.GetWeakPtr(), std::move(template_id)));
}

void SecurityKeysBioEnrollmentHandler::OnHavePostEnrollmentEnumeration(
    std::vector<uint8_t> enrolled_template_id,
    device::CtapDeviceResponseCode code,
    std::optional<std::map<std::vector<uint8_t>, std::string>> enrollments) {
  DCHECK_EQ(state_, State::kEnrolling);
  DCHECK(!callback_id_.empty());
  state_ = State::kReady;
  if (code != device::CtapDeviceResponseCode::kSuccess || !enrollments ||
      !base::Contains(*enrollments, enrolled_template_id)) {
    OnError(device::BioEnrollmentHandler::Error::kAuthenticatorResponseInvalid);
    return;
  }

  base::Value::Dict d;
  d.Set("code", static_cast<int>(code));
  d.Set("remaining", 0);
  d.Set("enrollment", EncodeEnrollment(enrolled_template_id,
                                       (*enrollments)[enrolled_template_id]));
  ResolveJavascriptCallback(base::Value(std::move(callback_id_)), d);
}

void SecurityKeysBioEnrollmentHandler::HandleDelete(
    const base::Value::List& args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(2u, args.size());
  state_ = State::kDeleting;
  callback_id_ = args[0].GetString();
  std::vector<uint8_t> template_id;
  if (!base::HexStringToBytes(args[1].GetString(), &template_id)) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  bio_->DeleteTemplate(
      std::move(template_id),
      base::BindOnce(&SecurityKeysBioEnrollmentHandler::OnDelete,
                     weak_factory_.GetWeakPtr()));
}

void SecurityKeysBioEnrollmentHandler::OnDelete(
    device::CtapDeviceResponseCode code) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(state_, State::kDeleting);
  DCHECK(!callback_id_.empty());
  state_ = State::kEnumerating;
  bio_->EnumerateTemplates(
      base::BindOnce(&SecurityKeysBioEnrollmentHandler::OnHaveEnumeration,
                     weak_factory_.GetWeakPtr()));
}

void SecurityKeysBioEnrollmentHandler::HandleRename(
    const base::Value::List& args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(args.size(), 3u);
  state_ = State::kRenaming;
  callback_id_ = args[0].GetString();
  std::vector<uint8_t> template_id;
  if (!base::HexStringToBytes(args[1].GetString(), &template_id)) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  bio_->RenameTemplate(
      std::move(template_id), args[2].GetString(),
      base::BindOnce(&SecurityKeysBioEnrollmentHandler::OnRename,
                     weak_factory_.GetWeakPtr()));
}

void SecurityKeysBioEnrollmentHandler::OnRename(
    device::CtapDeviceResponseCode code) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(state_, State::kRenaming);
  DCHECK(!callback_id_.empty());
  state_ = State::kEnumerating;
  bio_->EnumerateTemplates(
      base::BindOnce(&SecurityKeysBioEnrollmentHandler::OnHaveEnumeration,
                     weak_factory_.GetWeakPtr()));
}

void SecurityKeysBioEnrollmentHandler::HandleCancel(
    const base::Value::List& args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(state_, State::kEnrolling);
  DCHECK_EQ(0u, args.size());
  DCHECK(!callback_id_.empty());
  // OnEnrollmentFinished() will be invoked once the cancellation is complete.
  bio_->CancelEnrollment();
}

SecurityKeysPhonesHandler::SecurityKeysPhonesHandler() = default;
SecurityKeysPhonesHandler::~SecurityKeysPhonesHandler() = default;

void SecurityKeysPhonesHandler::OnJavascriptAllowed() {}
void SecurityKeysPhonesHandler::OnJavascriptDisallowed() {}

void SecurityKeysPhonesHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "securityKeyPhonesEnumerate",
      base::BindRepeating(&SecurityKeysPhonesHandler::HandleEnumerate,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "securityKeyPhonesDelete",
      base::BindRepeating(&SecurityKeysPhonesHandler::HandleDelete,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "securityKeyPhonesRename",
      base::BindRepeating(&SecurityKeysPhonesHandler::HandleRename,
                          base::Unretained(this)));
}

void SecurityKeysPhonesHandler::HandleEnumerate(const base::Value::List& args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(1u, args.size());

  AllowJavascript();
  DoEnumerate(args[0]);
}

void SecurityKeysPhonesHandler::HandleDelete(const base::Value::List& args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(2u, args.size());

  AllowJavascript();
  const std::string public_key_base64 = args[1].GetString();
  std::array<uint8_t, device::kP256X962Length> public_key;
  const bool ok = DecodePublicKey(public_key_base64, &public_key);
  DCHECK(ok);

  PrefService* const prefs =
      Profile::FromBrowserContext(
          web_ui()->GetWebContents()->GetBrowserContext())
          ->GetPrefs();
  cablev2::DeletePairingByPublicKey(prefs, public_key);

  DoEnumerate(args[0]);
}

void SecurityKeysPhonesHandler::HandleRename(const base::Value::List& args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(3u, args.size());

  AllowJavascript();
  const std::string public_key_base64 = args[1].GetString();
  const std::string new_name = args[2].GetString();
  content::BrowserContext* const browser_ctx =
      web_ui()->GetWebContents()->GetBrowserContext();

  std::array<uint8_t, device::kP256X962Length> public_key;
  const bool ok = DecodePublicKey(public_key_base64, &public_key);
  DCHECK(ok);

  // `existing_names` is built without calling `cablev2::MergeDevices` because
  // that function will discard linked entries with duplicate public keys, which
  // can hide some names that we would still like to avoid colliding with.
  std::unique_ptr<cablev2::KnownDevices> known_devices =
      cablev2::KnownDevices::FromProfile(
          Profile::FromBrowserContext(browser_ctx));

  // Remove the device that is getting renamed from the set of linked devices.
  std::erase_if(
      known_devices->linked_devices,
      [&public_key](const std::unique_ptr<device::cablev2::Pairing>& device) {
        return device->peer_public_key_x962 == public_key;
      });

  PrefService* const prefs =
      Profile::FromBrowserContext(browser_ctx)->GetPrefs();
  cablev2::RenamePairing(prefs, public_key, new_name, known_devices->Names());

  ResolveJavascriptCallback(args[0], base::Value());
}

void SecurityKeysPhonesHandler::DoEnumerate(const base::Value& callback_id) {
  const std::vector<std::unique_ptr<device::cablev2::Pairing>> pairings =
      cablev2::MergeDevices(
          cablev2::KnownDevices::FromProfile(Profile::FromBrowserContext(
              web_ui()->GetWebContents()->GetBrowserContext())),
          &icu::Locale::getDefault());

  base::Value::List synced;
  base::Value::List linked;
  std::optional<std::string> last_synced_device_name;
  for (const auto& pairing : pairings) {
    base::Value::Dict dict;
    dict.Set("name", pairing->name);
    dict.Set("publicKey", base::Base64Encode(pairing->peer_public_key_x962));

    if (pairing->from_sync_deviceinfo) {
      // Synced devices can have duplicate names. (E.g. if two or more
      // channels are syncing from the same phone.) These are deduplicated
      // here.
      if (!last_synced_device_name ||
          *last_synced_device_name != pairing->name) {
        synced.Append(std::move(dict));
      }
      last_synced_device_name = pairing->name;
    } else {
      linked.Append(std::move(dict));
    }
  }

  base::Value::List result;
  result.Append(std::move(synced));
  result.Append(std::move(linked));

  ResolveJavascriptCallback(callback_id, result);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

PasskeysHandler::PasskeysHandler() = default;

PasskeysHandler::PasskeysHandler(
    std::unique_ptr<LocalCredentialManagement> local_cred_man)
    : local_cred_man_(std::move(local_cred_man)) {}

PasskeysHandler::~PasskeysHandler() = default;

void PasskeysHandler::OnJavascriptAllowed() {}
void PasskeysHandler::OnJavascriptDisallowed() {
  weak_factory_.InvalidateWeakPtrs();
}

void PasskeysHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "passkeysHasPasskeys",
      base::BindRepeating(&PasskeysHandler::HandleHasPasskeys,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "passkeysManagePasskeys",
      base::BindRepeating(&PasskeysHandler::HandleManagePasskeys,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "passkeysEnumerate",
      base::BindRepeating(&PasskeysHandler::HandleEnumerate,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "passkeysDelete", base::BindRepeating(&PasskeysHandler::HandleDelete,
                                            base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "passkeysEdit", base::BindRepeating(&PasskeysHandler::HandleEdit,
                                          base::Unretained(this)));
  if (!local_cred_man_) {
    local_cred_man_ =
        LocalCredentialManagement::Create(Profile::FromBrowserContext(
            web_ui()->GetWebContents()->GetBrowserContext()));
  }
}

void PasskeysHandler::HandleHasPasskeys(const base::Value::List& args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(1u, args.size());

  AllowJavascript();
  local_cred_man_->HasCredentials(
      base::BindOnce(&PasskeysHandler::OnHasPasskeysComplete,
                     weak_factory_.GetWeakPtr(), args[0].GetString()));
}

void PasskeysHandler::OnHasPasskeysComplete(std::string callback_id,
                                            bool has_passkeys) {
  ResolveJavascriptCallback(base::Value(std::move(callback_id)),
                            base::Value(has_passkeys));
}

void PasskeysHandler::HandleManagePasskeys(const base::Value::List& args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(0u, args.size());

  AllowJavascript();

#if BUILDFLAG(IS_WIN)
  auto* windows_api = device::WinWebAuthnApi::GetDefault();
  // webauthn.dll version six includes management support, so if at least that
  // version is found then Windows does management natively.
  constexpr int kWebAuthnDLLWithManagementSupport = 6;
  if (windows_api->IsAvailable() &&
      windows_api->Version() >= kWebAuthnDLLWithManagementSupport) {
    platform_util::OpenExternal(GURL("ms-settings:savedpasskeys"));
    return;
  }
#endif

  // If no system management exists, fall back to Chrome's own settings UI.
  chrome::ShowSettingsSubPage(
      chrome::FindBrowserWithTab(web_ui()->GetWebContents()),
      chrome::kPasskeysSubPage);
}

void PasskeysHandler::HandleEnumerate(const base::Value::List& args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(1u, args.size());

  AllowJavascript();
  DoEnumerate(args[0].GetString());
}

void PasskeysHandler::DoEnumerate(std::string callback_id) {
  local_cred_man_->Enumerate(
      base::BindOnce(&PasskeysHandler::OnEnumerateComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback_id)));
}

void PasskeysHandler::OnEnumerateComplete(
    std::string callback_id,
    std::optional<std::vector<device::DiscoverableCredentialMetadata>>
        credentials) {
  base::Value result;

  if (credentials.has_value()) {
    base::Value::List passkeys;
    for (const auto& cred : *credentials) {
      // RP IDs with colons in them are assumed to be URLs and thus for
      // protocols like SSH. In order to avoid confusing the UI, these are
      // filtered out.
      if (cred.rp_id.find(':') != std::string::npos) {
        continue;
      }

      base::Value::Dict passkey;
      passkey.Set("relyingPartyId", cred.rp_id);
      passkey.Set("userName", cred.user.name.value_or(""));
      passkey.Set("userDisplayName", cred.user.display_name.value_or(""));
      passkey.Set("credentialId", base::HexEncode(cred.cred_id));
      passkeys.Append(std::move(passkey));
    }

    result = base::Value(std::move(passkeys));
  }

  ResolveJavascriptCallback(base::Value(std::move(callback_id)), result);
}

void PasskeysHandler::HandleDelete(const base::Value::List& args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(2u, args.size());

  AllowJavascript();
  std::vector<uint8_t> credential_id;
  const bool ok = base::HexStringToBytes(args[1].GetString(), &credential_id);
  DCHECK(ok);

  local_cred_man_->Delete(
      credential_id,
      base::BindOnce(&PasskeysHandler::OnDeleteComplete,
                     weak_factory_.GetWeakPtr(), args[0].GetString()));
}

void PasskeysHandler::OnDeleteComplete(std::string callback_id, bool ok) {
  // The ok parameter is ignored. If it were false, it would mean
  // Windows/Mac failed to delete the passkey. This can happen if API support
  // is missing but no passkeys will be shown at all in that case so that
  // should be impossible. It can also happen if the user attempts to delete a
  // system-created credential. In this case the Javascript will notice that
  // the credential didn't disappear and will show an error message.
  DoEnumerate(std::move(callback_id));
}

void PasskeysHandler::HandleEdit(const base::Value::List& args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(3u, args.size());

  AllowJavascript();
  std::vector<uint8_t> credential_id;
  const bool ok = base::HexStringToBytes(args[1].GetString(), &credential_id);
  DCHECK(ok);

  std::string new_username = args[2].GetString();
  local_cred_man_->Edit(
      credential_id, std::move(new_username),
      base::BindOnce(&PasskeysHandler::OnEditComplete,
                     weak_factory_.GetWeakPtr(), args[0].GetString()));
}

void PasskeysHandler::OnEditComplete(std::string callback_id, bool ok) {
  // The ok parameter is ignored. If it were false, it would mean
  // Windows/Mac failed to edit the passkey.
  DoEnumerate(std::move(callback_id));
}
#endif

}  // namespace settings
