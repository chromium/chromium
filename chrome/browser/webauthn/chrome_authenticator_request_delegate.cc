// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/permissions/attestation_permission_request.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webauthn/authenticator_request_dialog.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

#if defined(OS_MACOSX)
#include "device/fido/mac/credential_metadata.h"
#endif

namespace {

// Returns true iff |relying_party_id| is listed in the
// SecurityKeyPermitAttestation policy.
bool IsWebauthnRPIDListedInEnterprisePolicy(
    content::BrowserContext* browser_context,
    const std::string& relying_party_id) {
#if defined(OS_ANDROID)
  return false;
#else
  const Profile* profile = Profile::FromBrowserContext(browser_context);
  const PrefService* prefs = profile->GetPrefs();
  const base::ListValue* permit_attestation =
      prefs->GetList(prefs::kSecurityKeyPermitAttestation);
  return std::any_of(permit_attestation->begin(), permit_attestation->end(),
                     [&relying_party_id](const base::Value& v) {
                       return v.GetString() == relying_party_id;
                     });
#endif
}

bool IsWebAuthnUiEnabled() {
  return base::FeatureList::IsEnabled(features::kWebAuthenticationUI);
}

}  // namespace

#if defined(OS_MACOSX)
static const char kWebAuthnTouchIdMetadataSecretPrefName[] =
    "webauthn.touchid.metadata_secret";
#endif

static const char kWebAuthnLastTransportUsedPrefName[] =
    "webauthn.last_transport_used";

static const char kWebAuthnBlePairedMacAddressesPrefName[] =
    "webauthn.ble.paired_mac_addresses";

// static
void ChromeAuthenticatorRequestDelegate::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
#if defined(OS_MACOSX)
  registry->RegisterStringPref(kWebAuthnTouchIdMetadataSecretPrefName,
                               std::string());
#endif

  registry->RegisterStringPref(kWebAuthnLastTransportUsedPrefName,
                               std::string());
  registry->RegisterListPref(kWebAuthnBlePairedMacAddressesPrefName,
                             std::make_unique<base::ListValue>());
}

ChromeAuthenticatorRequestDelegate::ChromeAuthenticatorRequestDelegate(
    content::RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host), weak_ptr_factory_(this) {}

ChromeAuthenticatorRequestDelegate::~ChromeAuthenticatorRequestDelegate() {
  // Currently, completion of the request is indicated by //content destroying
  // this delegate.
  if (weak_dialog_model_) {
    weak_dialog_model_->OnRequestComplete();
  }

  // The dialog model may be destroyed after the OnRequestComplete call.
  if (weak_dialog_model_) {
    weak_dialog_model_->RemoveObserver(this);
    weak_dialog_model_ = nullptr;
  }
}

base::WeakPtr<ChromeAuthenticatorRequestDelegate>
ChromeAuthenticatorRequestDelegate::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

content::BrowserContext* ChromeAuthenticatorRequestDelegate::browser_context()
    const {
  return content::WebContents::FromRenderFrameHost(render_frame_host())
      ->GetBrowserContext();
}

void ChromeAuthenticatorRequestDelegate::DidFailWithInterestingReason(
    InterestingFailureReason reason) {
  if (!weak_dialog_model_)
    return;

  switch (reason) {
    case InterestingFailureReason::kTimeout:
      weak_dialog_model_->OnRequestTimeout();
      break;
    case InterestingFailureReason::kKeyNotRegistered:
      weak_dialog_model_->OnActivatedKeyNotRegistered();
      break;
    case InterestingFailureReason::kKeyAlreadyRegistered:
      weak_dialog_model_->OnActivatedKeyAlreadyRegistered();
      break;
  }
}

void ChromeAuthenticatorRequestDelegate::RegisterActionCallbacks(
    base::OnceClosure cancel_callback,
    device::FidoRequestHandlerBase::RequestCallback request_callback,
    base::RepeatingClosure bluetooth_adapter_power_on_callback,
    device::FidoRequestHandlerBase::BlePairingCallback ble_pairing_callback) {
  request_callback_ = request_callback;
  cancel_callback_ = std::move(cancel_callback);

  transient_dialog_model_holder_ =
      std::make_unique<AuthenticatorRequestDialogModel>();
  transient_dialog_model_holder_->SetRequestCallback(request_callback);
  transient_dialog_model_holder_->SetBluetoothAdapterPowerOnCallback(
      bluetooth_adapter_power_on_callback);
  transient_dialog_model_holder_->SetBlePairingCallback(ble_pairing_callback);

  weak_dialog_model_ = transient_dialog_model_holder_.get();
  weak_dialog_model_->AddObserver(this);
}

bool ChromeAuthenticatorRequestDelegate::ShouldPermitIndividualAttestation(
    const std::string& relying_party_id) {
  // If the RP ID is listed in the policy, signal that individual attestation is
  // permitted.
  return IsWebauthnRPIDListedInEnterprisePolicy(browser_context(),
                                                relying_party_id);
}

void ChromeAuthenticatorRequestDelegate::ShouldReturnAttestation(
    const std::string& relying_party_id,
    base::OnceCallback<void(bool)> callback) {
#if defined(OS_ANDROID)
  // Android is expected to use platform APIs for webauthn which will take care
  // of prompting.
  std::move(callback).Run(true);
#else
  if (IsWebauthnRPIDListedInEnterprisePolicy(browser_context(),
                                             relying_party_id)) {
    std::move(callback).Run(true);
    return;
  }

  // This does not use content::PermissionControllerDelegate because that only
  // works with content settings, while this permission is a non-persisted,
  // per-attested- registration consent.
  auto* permission_request_manager = PermissionRequestManager::FromWebContents(
      content::WebContents::FromRenderFrameHost(render_frame_host()));
  if (!permission_request_manager) {
    std::move(callback).Run(false);
    return;
  }

  // The created AttestationPermissionRequest deletes itself once complete.
  //
  // |callback| is called via the |MessageLoop| because otherwise the
  // permissions bubble will have focus and |AuthenticatorImpl| checks that the
  // frame still has focus before returning any results.
  permission_request_manager->AddRequest(NewAttestationPermissionRequest(
      render_frame_host()->GetLastCommittedOrigin(),
      base::BindOnce(
          [](base::OnceCallback<void(bool)> callback, bool result) {
            base::ThreadTaskRunnerHandle::Get()->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback), result));
          },
          std::move(callback))));
#endif
}

bool ChromeAuthenticatorRequestDelegate::IsFocused() {
#if defined(OS_ANDROID)
  // Android is expected to use platform APIs for webauthn.
  return true;
#else
  auto* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host());
  DCHECK(web_contents);
  return web_contents->GetVisibility() == content::Visibility::VISIBLE;
#endif
}

#if defined(OS_MACOSX)
static constexpr char kTouchIdKeychainAccessGroup[] =
    "EQHXZ8M8AV.com.google.Chrome.webauthn";

namespace {

std::string TouchIdMetadataSecret(Profile* profile) {
  PrefService* prefs = profile->GetPrefs();
  std::string key = prefs->GetString(kWebAuthnTouchIdMetadataSecretPrefName);
  if (key.empty() || !base::Base64Decode(key, &key)) {
    key = device::fido::mac::CredentialMetadata::GenerateRandomSecret();
    std::string encoded_key;
    base::Base64Encode(key, &encoded_key);
    prefs->SetString(kWebAuthnTouchIdMetadataSecretPrefName, encoded_key);
  }
  return key;
}

}  // namespace

// static
content::AuthenticatorRequestClientDelegate::TouchIdAuthenticatorConfig
ChromeAuthenticatorRequestDelegate::TouchIdAuthenticatorConfigForProfile(
    Profile* profile) {
  return content::AuthenticatorRequestClientDelegate::
      TouchIdAuthenticatorConfig{kTouchIdKeychainAccessGroup,
                                 TouchIdMetadataSecret(profile)};
}

base::Optional<
    content::AuthenticatorRequestClientDelegate::TouchIdAuthenticatorConfig>
ChromeAuthenticatorRequestDelegate::GetTouchIdAuthenticatorConfig() const {
  return TouchIdAuthenticatorConfigForProfile(
      Profile::FromBrowserContext(browser_context()));
}
#endif

void ChromeAuthenticatorRequestDelegate::UpdateLastTransportUsed(
    device::FidoTransportProtocol transport) {
  PrefService* prefs =
      Profile::FromBrowserContext(browser_context())->GetPrefs();
  prefs->SetString(kWebAuthnLastTransportUsedPrefName,
                   device::ToString(transport));
}

void ChromeAuthenticatorRequestDelegate::OnTransportAvailabilityEnumerated(
    device::FidoRequestHandlerBase::TransportAvailabilityInfo data) {
#if !defined(OS_ANDROID)
  if (!IsWebAuthnUiEnabled())
    return;

  DCHECK(weak_dialog_model_);
  weak_dialog_model_->StartFlow(std::move(data), GetLastTransportUsed());

  DCHECK(transient_dialog_model_holder_);
  ShowAuthenticatorRequestDialog(
      content::WebContents::FromRenderFrameHost(render_frame_host()),
      std::move(transient_dialog_model_holder_));
#endif
}

bool ChromeAuthenticatorRequestDelegate::EmbedderControlsAuthenticatorDispatch(
    const device::FidoAuthenticator& authenticator) {
  // TODO(hongjunchoi): Change this so that requests for BLE authenticators are
  // not dispatched immediately if WebAuthN UI is enabled.
  if (!IsWebAuthnUiEnabled())
    return false;

  return authenticator.AuthenticatorTransport() ==
         device::FidoTransportProtocol::kInternal;
}

void ChromeAuthenticatorRequestDelegate::FidoAuthenticatorAdded(
    const device::FidoAuthenticator& authenticator) {
  if (!IsWebAuthnUiEnabled())
    return;

  if (!weak_dialog_model_)
    return;

  weak_dialog_model_->AddAuthenticator(authenticator);
}

void ChromeAuthenticatorRequestDelegate::FidoAuthenticatorRemoved(
    base::StringPiece authenticator_id) {
  if (!IsWebAuthnUiEnabled())
    return;

  if (!weak_dialog_model_)
    return;

  weak_dialog_model_->RemoveAuthenticator(authenticator_id);
}

void ChromeAuthenticatorRequestDelegate::FidoAuthenticatorIdChanged(
    base::StringPiece old_authenticator_id,
    std::string new_authenticator_id) {
  if (!weak_dialog_model_)
    return;

  weak_dialog_model_->UpdateAuthenticatorReferenceId(
      old_authenticator_id, std::move(new_authenticator_id));
}

void ChromeAuthenticatorRequestDelegate::FidoAuthenticatorPairingModeChanged(
    base::StringPiece authenticator_id,
    bool is_in_pairing_mode) {
  if (!weak_dialog_model_)
    return;

  weak_dialog_model_->UpdateAuthenticatorReferencePairingMode(
      authenticator_id, is_in_pairing_mode);
}

void ChromeAuthenticatorRequestDelegate::BluetoothAdapterPowerChanged(
    bool is_powered_on) {
  if (!weak_dialog_model_)
    return;

  weak_dialog_model_->OnBluetoothPoweredStateChanged(is_powered_on);
}
void ChromeAuthenticatorRequestDelegate::OnModelDestroyed() {
  DCHECK(weak_dialog_model_);
  weak_dialog_model_ = nullptr;
}

void ChromeAuthenticatorRequestDelegate::OnCancelRequest() {
  // |cancel_callback_| must be invoked at most once as invocation of
  // |cancel_callback_| will destroy |this|.
  DCHECK(cancel_callback_);
  std::move(cancel_callback_).Run();
}

void ChromeAuthenticatorRequestDelegate::AddFidoBleDeviceToPairedList(
    std::string device_address) {
  ListPrefUpdate update(
      Profile::FromBrowserContext(browser_context())->GetPrefs(),
      kWebAuthnBlePairedMacAddressesPrefName);
  bool already_contains_address = std::any_of(
      update->begin(), update->end(), [&device_address](const auto& value) {
        return value.is_string() && value.GetString() == device_address;
      });

  if (already_contains_address)
    return;

  update->Append(std::make_unique<base::Value>(std::move(device_address)));
}

base::Optional<device::FidoTransportProtocol>
ChromeAuthenticatorRequestDelegate::GetLastTransportUsed() const {
  PrefService* prefs =
      Profile::FromBrowserContext(browser_context())->GetPrefs();
  return device::ConvertToFidoTransportProtocol(
      prefs->GetString(kWebAuthnLastTransportUsedPrefName));
}

const base::ListValue*
ChromeAuthenticatorRequestDelegate::GetPreviouslyPairedFidoBleDeviceAddresses()
    const {
  PrefService* prefs =
      Profile::FromBrowserContext(browser_context())->GetPrefs();
  return prefs->GetList(kWebAuthnBlePairedMacAddressesPrefName);
}
