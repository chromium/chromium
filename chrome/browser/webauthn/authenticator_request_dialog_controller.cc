// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/authenticator_request_dialog_controller.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/string_compare.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/webauthn/ambient/ambient_signin_controller.h"
#include "chrome/browser/ui/webauthn/passkey_upgrade_request_controller.h"
#include "chrome/browser/ui/webauthn/user_actions.h"
#include "chrome/browser/webauthn/authenticator_reference.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/authenticator_transport.h"
#include "chrome/browser/webauthn/challenge_url_fetcher.h"
#include "chrome/browser/webauthn/change_pin_controller_impl.h"
#include "chrome/browser/webauthn/credential_sorter_desktop.h"
#include "chrome/browser/webauthn/gpm_enclave_transaction.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "chrome/browser/webauthn/password_credential_fetcher.h"
#include "chrome/browser/webauthn/webauthn_metrics_util.h"
#include "chrome/browser/webauthn/webauthn_pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/device_event_log/device_event_log.h"
#include "components/password_manager/core/browser/credential_manager_utils.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "components/webauthn/core/browser/gpm_user_verification_policy.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "components/webauthn/core/browser/passkey_model_change.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/enclave/constants.h"
#include "device/fido/enclave/metrics.h"
#include "device/fido/features.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/pin.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "third_party/icu/source/i18n/unicode/coll.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_elider.h"
#include "url/scheme_host_port.h"

#if BUILDFLAG(IS_WIN)
#include "device/fido/win/webauthn_api.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#include "device/fido/mac/util.h"
#endif

namespace {

constexpr int kMaxPriorityGPMCredentialCreations = 2;

using BleStatus = device::FidoRequestHandlerBase::BleStatus;
using ChangePinEvent = ChangePinControllerImpl::ChangePinEvent;
using Mechanism = AuthenticatorRequestDialogModel::Mechanism;
using Step = AuthenticatorRequestDialogModel::Step;
using TransportAvailabilityInfo =
    device::FidoRequestHandlerBase::TransportAvailabilityInfo;
using UIPresentation =
    content::AuthenticatorRequestClientDelegate::UIPresentation;
using device::AuthenticatorType;
using device::FidoRequestType;
using PasswordCredentials = PasswordCredentialFetcher::PasswordCredentials;

constexpr int GetMessageIdForTransportDescription(
    AuthenticatorTransport transport) {
  switch (transport) {
    case AuthenticatorTransport::kUsbHumanInterfaceDevice:
      return IDS_WEBAUTHN_TRANSPORT_USB;
    case AuthenticatorTransport::kInternal:
#if BUILDFLAG(IS_MAC)
      return IDS_WEBAUTHN_YOUR_CHROME_PROFILE;
#else
      return IDS_WEBAUTHN_TRANSPORT_INTERNAL;
#endif
    case AuthenticatorTransport::kHybrid:
      return IDS_WEBAUTHN_TRANSPORT_CABLE;
    case AuthenticatorTransport::kDeprecatedAoa:
    case AuthenticatorTransport::kBluetoothLowEnergy:
    case AuthenticatorTransport::kNearFieldCommunication:
      NOTREACHED();
  }
}

std::u16string GetTransportDescription(AuthenticatorTransport transport) {
  const int msg_id = GetMessageIdForTransportDescription(transport);
  if (!msg_id) {
    return std::u16string();
  }
  return l10n_util::GetStringUTF16(msg_id);
}

constexpr const gfx::VectorIcon& GetTransportIcon(
    AuthenticatorTransport transport) {
  switch (transport) {
    case AuthenticatorTransport::kUsbHumanInterfaceDevice:
      return kUsbSecurityKeyIcon;
    case AuthenticatorTransport::kInternal:
      return kLaptopIcon;
    case AuthenticatorTransport::kHybrid:
      return kSmartphoneIcon;
    case AuthenticatorTransport::kDeprecatedAoa:
    case AuthenticatorTransport::kBluetoothLowEnergy:
    case AuthenticatorTransport::kNearFieldCommunication:
      NOTREACHED();
  }
}

// Whether to show Step::kCreatePasskey, which prompts the user before platform
// authenticator dispatch during MakeCredential. This is currently only shown on
// MacOS, because that is the only desktop platform authenticator without a
// "native" WebAuthn UI.
constexpr bool kShowCreatePlatformPasskeyStep = BUILDFLAG(IS_MAC);

password_manager::PasskeyCredential::Source ToPasswordManagerSource(
    AuthenticatorType type) {
  switch (type) {
    case AuthenticatorType::kWinNative:
      return password_manager::PasskeyCredential::Source::kWindowsHello;
    case AuthenticatorType::kTouchID:
      return password_manager::PasskeyCredential::Source::kTouchId;
    case AuthenticatorType::kPhone:
      return password_manager::PasskeyCredential::Source::kAndroidPhone;
    case AuthenticatorType::kICloudKeychain:
      return password_manager::PasskeyCredential::Source::kICloudKeychain;
    case AuthenticatorType::kEnclave:
      return password_manager::PasskeyCredential::Source::
          kGooglePasswordManager;
    case AuthenticatorType::kChromeOS:
    case AuthenticatorType::kOther:
      return password_manager::PasskeyCredential::Source::kOther;
  }
}

bool WebAuthnApiSupportsHybrid() {
#if BUILDFLAG(IS_WIN)
  device::WinWebAuthnApi* const webauthn_api =
      device::WinWebAuthnApi::GetDefault();
  return webauthn_api && webauthn_api->SupportsHybrid();
#else
  return false;
#endif
}

const gfx::VectorIcon& GetCredentialIcon(AuthenticatorType type) {
  if (type == AuthenticatorType::kPhone) {
    return kSmartphoneIcon;
  }
  return vector_icons::kPasskeyIcon;
}

int GetHybridButtonLabel(bool has_security_key) {
  return has_security_key
             ? IDS_WEBAUTHN_PASSKEY_PHONE_TABLET_OR_SECURITY_KEY_LABEL
             : IDS_WEBAUTHN_PASSKEY_PHONE_OR_TABLET_LABEL;
}

// SourcePriority determines which credential will be used when doing a modal
// get and multiple platform authenticators have credentials, all with the same
// user ID.
int SourcePriority(AuthenticatorType source) {
  switch (source) {
    case AuthenticatorType::kEnclave:
      return 4;
    case AuthenticatorType::kICloudKeychain:
      return 3;
    case AuthenticatorType::kTouchID:
      return 2;
    case AuthenticatorType::kWinNative:
      return 1;
    default:
      return 0;
  }
}

// Returns the ID of a string and authenticator transport to label a button that
// triggers the Windows native WebAuthn API, or std::nullopt if the button
// should not be shown. The transport represents the option Windows will prefer
// when tapping the button and is used to pick an icon and position on the list.
std::optional<std::pair<int, AuthenticatorTransport>> GetWindowsAPIButtonLabel(
    const device::FidoRequestHandlerBase::TransportAvailabilityInfo&
        transport_availability,
    UIPresentation ui_presentation) {
  if (ui_presentation == UIPresentation::kModalImmediate) {
    return std::nullopt;
  }
  if (!transport_availability.has_win_native_api_authenticator) {
    return std::nullopt;
  }
  bool win_handles_internal;
  bool win_handles_hybrid;
  bool win_handles_security_key;
  if (transport_availability.request_type == FidoRequestType::kGetAssertion) {
    win_handles_internal =
        (transport_availability.transport_list_did_include_internal ||
         transport_availability.has_empty_allow_list) &&
        transport_availability.has_platform_authenticator_credential ==
            device::FidoRequestHandlerBase::RecognizedCredential::kUnknown &&
        transport_availability.win_is_uvpaa;
    win_handles_hybrid =
        (transport_availability.transport_list_did_include_hybrid ||
         transport_availability.has_empty_allow_list) &&
        WebAuthnApiSupportsHybrid();
    win_handles_security_key =
        transport_availability.transport_list_did_include_security_key ||
        transport_availability.has_empty_allow_list;
  } else {
    win_handles_internal = (transport_availability.make_credential_attachment ==
                                device::AuthenticatorAttachment::kPlatform ||
                            transport_availability.make_credential_attachment ==
                                device::AuthenticatorAttachment::kAny) &&
                           transport_availability.win_is_uvpaa;
    win_handles_security_key =
        transport_availability.make_credential_attachment ==
            device::AuthenticatorAttachment::kCrossPlatform ||
        transport_availability.make_credential_attachment ==
            device::AuthenticatorAttachment::kAny;
    win_handles_hybrid =
        WebAuthnApiSupportsHybrid() && win_handles_security_key;
  }
  if (win_handles_internal) {
    if (win_handles_security_key) {
      return std::make_pair(
          IDS_WEBAUTHN_TRANSPORT_WINDOWS_HELLO_OR_SECURITY_KEY,
          AuthenticatorTransport::kInternal);
    } else {
      return std::make_pair(IDS_WEBAUTHN_TRANSPORT_WINDOWS_HELLO,
                            AuthenticatorTransport::kInternal);
    }
  }
  if (win_handles_hybrid) {
    return std::make_pair(GetHybridButtonLabel(win_handles_security_key),
                          AuthenticatorTransport::kHybrid);
  }
  if (win_handles_security_key) {
    return std::make_pair(IDS_WEBAUTHN_TRANSPORT_EXTERNAL_SECURITY_KEY,
                          AuthenticatorTransport::kUsbHumanInterfaceDevice);
  }
  return std::nullopt;
}

// Returns whether the given authenticator type is implemented within Chrome
// itself for the purposes of `StartPlatformAuthenticatorFlow`.
bool IsChromeImplemented(AuthenticatorType type) {
  // Note: it must never be possible for any machine to observe two different
  // sources of "Chrome implemented" credentials. I.e. a given platform only
  // ever has one type of Chrome-implemented platform authenticator.
  // This is CHECKed in `StartFlow`.
  switch (type) {
    case AuthenticatorType::kWinNative:
    case AuthenticatorType::kPhone:
    case AuthenticatorType::kEnclave:
    case AuthenticatorType::kICloudKeychain:
      return false;
    case AuthenticatorType::kTouchID:
    case AuthenticatorType::kChromeOS:
      return true;
    case AuthenticatorType::kOther:
      // For testing purposes.
      return true;
  }
}

bool ProfileAuthenticatorWillDoUserVerification(
    device::UserVerificationRequirement requirement,
    bool platform_has_biometrics) {
#if BUILDFLAG(IS_MAC)
  return device::fido::mac::ProfileAuthenticatorWillDoUserVerification(
      requirement, platform_has_biometrics);
#else
  return false;
#endif
}

inline bool IsModalRequest(UIPresentation ui_presentation) {
  return ui_presentation == UIPresentation::kModal ||
         ui_presentation == UIPresentation::kModalImmediate;
}

// Returns the vector icon associated with the given mechanism type.
// For Mechanism::WindowsAPI, the effective transport must be provided.
const gfx::VectorIcon& GetMechanismIcon(
    const Mechanism::Type& type,
    content::AuthenticatorRequestClientDelegate::UIPresentation ui_presentation,
    std::optional<AuthenticatorTransport> effective_transport = std::nullopt) {
  return std::visit(
      absl::Overload{
          [ui_presentation](const Mechanism::Credential& credential)
              -> const gfx::VectorIcon& {
            if (ui_presentation == UIPresentation::kModalImmediate) {
              switch (credential.value().source) {
                case AuthenticatorType::kICloudKeychain:
                  return kIcloudKeychainColorIcon;
                case AuthenticatorType::kEnclave:
                  return GooglePasswordManagerVectorIcon();
                case AuthenticatorType::kWinNative:
                  return kWindowsHelloColorIcon;
                case AuthenticatorType::kTouchID:
                  return vector_icons::kProductRefreshIcon;
                default:
                  break;
              }
            }
            // Default icon for non-immediate mode or other credential sources.
            return GetCredentialIcon(credential.value().source);
          },
          [](const Mechanism::Password&) -> const gfx::VectorIcon& {
            return GooglePasswordManagerVectorIcon();
          },
          [](const Mechanism::Transport& transport) -> const gfx::VectorIcon& {
            return GetTransportIcon(transport.value());
          },
          [&effective_transport](
              const Mechanism::WindowsAPI&) -> const gfx::VectorIcon& {
            CHECK(effective_transport.has_value());
            return GetTransportIcon(*effective_transport);
          },
          [](const Mechanism::ICloudKeychain&) -> const gfx::VectorIcon& {
            // Always use the standard iCloud Keychain icon here.
            return kIcloudKeychainIcon;
          },
          [](const Mechanism::Hybrid&) -> const gfx::VectorIcon& {
            return kQrcodeGeneratorIcon;
          },
          [](const Mechanism::Enclave&) -> const gfx::VectorIcon& {
            // Always use the standard password manager icon here.
            return vector_icons::kPasswordManagerIcon;
          },
          [](const Mechanism::SignInAgain&) -> const gfx::VectorIcon& {
            return vector_icons::kSyncIcon;
          }},
      type);
}

// Returns `true` if `mech` satisfies the given `hint`, `false` otherwise.
bool MechanismMatchesHint(const Mechanism::Type& mech,
                          AuthenticatorTransport hint) {
  return std::visit(
      absl::Overload{
          [hint](const Mechanism::Transport& transport) {
            return transport.value() == hint;
          },
          [hint](const Mechanism::Hybrid&) {
            return hint == AuthenticatorTransport::kHybrid;
          },
          [hint](const Mechanism::Enclave&) {
            return hint == AuthenticatorTransport::kInternal;
          },
          [hint](const Mechanism::ICloudKeychain&) {
            return hint == AuthenticatorTransport::kInternal;
          },
          [hint](const Mechanism::WindowsAPI&) {
            return hint == AuthenticatorTransport::kInternal ||
                   hint == AuthenticatorTransport::kUsbHumanInterfaceDevice ||
                   (hint == AuthenticatorTransport::kHybrid &&
                    WebAuthnApiSupportsHybrid());
          },
          [](const Mechanism::Credential&) {
            // Credentials are always given priority over hints.
            return false;
          },
          [](const Mechanism::Password&) { return false; },
          [](const Mechanism::SignInAgain&) { return false; },
      },
      mech);
}

// Returns the index of the first mechanism that matches `type`, or std::nullopt
// if none is found.
std::optional<int> FindIndexOfFirstMechanismOfType(
    base::span<const Mechanism> mechanisms,
    const Mechanism::Type& type) {
  for (size_t i = 0; i < mechanisms.size(); i++) {
    if (type == mechanisms[i].type) {
      return i;
    }
  }
  return std::nullopt;
}

// Returns `true` if there are credentials in `mechanisms`, and they all
// correspond to Windows Hello.
bool AreAllCredentialsWindowsHello(const std::vector<Mechanism>& mechanisms) {
  std::vector<const Mechanism::Credential*> credentials;
  for (const auto& mech : mechanisms) {
    if (std::holds_alternative<Mechanism::Credential>(mech.type)) {
      credentials.push_back(&std::get<Mechanism::Credential>(mech.type));
    }
  }
  if (credentials.empty()) {
    return false;
  }
  return std::ranges::all_of(credentials, [](const auto* cred) {
    return cred->value().source == AuthenticatorType::kWinNative;
  });
}

}  // namespace

AuthenticatorRequestDialogController::EphemeralState::EphemeralState() =
    default;
AuthenticatorRequestDialogController::EphemeralState::EphemeralState(
    EphemeralState&&) = default;
AuthenticatorRequestDialogController::EphemeralState&
AuthenticatorRequestDialogController::EphemeralState::operator=(
    EphemeralState&&) = default;
AuthenticatorRequestDialogController::EphemeralState::~EphemeralState() =
    default;

void AuthenticatorRequestDialogController::ResetEphemeralState() {
  ephemeral_state_ = {};
  model_->creds.clear();
  model_->priority_mechanism_index.reset();
}

AuthenticatorRequestDialogController::AuthenticatorRequestDialogController(
    AuthenticatorRequestDialogModel* model,
    content::RenderFrameHost* render_frame_host)
    : model_(model), frame_host_id_(render_frame_host->GetGlobalId()) {
  model_->observers.AddObserver(this);
  webauthn::PasskeyModel* passkey_model =
      PasskeyModelFactory::GetInstance()->GetForProfile(
          Profile::FromBrowserContext(render_frame_host->GetBrowserContext()));
  if (passkey_model) {
    passkey_model_observation_.Observe(passkey_model);
  }
}

AuthenticatorRequestDialogController::~AuthenticatorRequestDialogController() {
  if (model_) {
    model_->observers.RemoveObserver(this);
  }
}

AuthenticatorRequestDialogModel* AuthenticatorRequestDialogController::model()
    const {
  return model_;
}

void AuthenticatorRequestDialogController::OnModelDestroyed(
    AuthenticatorRequestDialogModel* model) {
  // This stops the destructor of this object from trying to remove itself from
  // the list of observers. But this is not a valid state for this object to be
  // in: many functions will crash. So this is just to make destroying the two
  // objects together not depend on the order of destruction.
  CHECK_EQ(model, model_);
  model_ = nullptr;
}

void AuthenticatorRequestDialogController::StartOver() {
  PrefService* pref_service =
      Profile::FromBrowserContext(GetRenderFrameHost()->GetBrowserContext())
          ->GetOriginalProfile()
          ->GetPrefs();
  if (model_->step() == Step::kTrustThisComputerCreation ||
      model_->step() == Step::kTrustThisComputerAssertion ||
      model_->step() == Step::kRecoverSecurityDomain) {
    device::enclave::RecordEvent(device::enclave::Event::kOnboardingRejected);
    int current_gpm_decline_count = pref_service->GetInteger(
        webauthn::pref_names::kEnclaveDeclinedGPMBootstrappingCount);
    pref_service->SetInteger(
        webauthn::pref_names::kEnclaveDeclinedGPMBootstrappingCount,
        std::min(current_gpm_decline_count + 1,
                 device::enclave::kMaxGPMBootstrapPrompts));
  } else if (enclave_was_priority_mechanism_) {
    int current_gpm_decline_count = pref_service->GetInteger(
        webauthn::pref_names::kEnclaveDeclinedGPMCredentialCreationCount);
    pref_service->SetInteger(
        webauthn::pref_names::kEnclaveDeclinedGPMCredentialCreationCount,
        std::min(current_gpm_decline_count + 1,
                 kMaxPriorityGPMCredentialCreations));
    device::enclave::RecordEvent(
        device::enclave::Event::kMakeCredentialPriorityDeclined);
    enclave_was_priority_mechanism_ = false;
  }
  ResetEphemeralState();

  for (auto& observer : model_->observers) {
    observer.OnStartOver();
  }
  SetCurrentStep(Step::kMechanismSelection);
}

void AuthenticatorRequestDialogController::OnCreatePasskeyAccepted() {
  HideDialogAndDispatchToPlatformAuthenticator();
}

void AuthenticatorRequestDialogController::OnRecoverSecurityDomainClosed() {
  if (model_->step() == Step::kGPMReauthForPinReset) {
    ChangePinControllerImpl::RecordHistogram(ChangePinEvent::kReauthCancelled);
  }
  // For modal get requests, fallback to the credential selector if the user
  // dismissed the recovery window. This will ensure the users to have a backup
  // such as hybrid.
  if (transport_availability_.request_type == FidoRequestType::kGetAssertion &&
      IsModalRequest(ui_presentation()) &&
      model_->step() == Step::kRecoverSecurityDomain) {
    model_->StartOver();
    return;
  }
  CancelAuthenticatorRequest();
}

void AuthenticatorRequestDialogController::
    ContinueWithFlowAfterBleAdapterPowered() {
  DCHECK(model_->step() == Step::kBlePowerOnManual ||
         model_->step() == Step::kBlePowerOnAutomatic);
  DCHECK(model_->ble_adapter_is_powered);

  std::move(after_ble_adapter_powered_).Run();
}

void AuthenticatorRequestDialogController::PowerOnBleAdapter() {
  DCHECK_EQ(model_->step(), Step::kBlePowerOnAutomatic);
  if (!bluetooth_adapter_power_on_callback_) {
    return;
  }

  bluetooth_adapter_power_on_callback_.Run();
}

void AuthenticatorRequestDialogController::OpenBlePreferences() {
#if BUILDFLAG(IS_MAC)
  DCHECK_EQ(model_->step(), Step::kBlePermissionMac);
  base::mac::OpenSystemSettingsPane(
      base::mac::SystemSettingsPane::kPrivacySecurity_Bluetooth);
#endif  // IS_MAC
}

void AuthenticatorRequestDialogController::
    OnOffTheRecordInterstitialAccepted() {
  std::move(after_off_the_record_interstitial_).Run();
}

void AuthenticatorRequestDialogController::CancelAuthenticatorRequest() {
  if (model_->step() == Step::kGPMChangeArbitraryPin ||
      model_->step() == Step::kGPMChangePin) {
    ChangePinControllerImpl::RecordHistogram(ChangePinEvent::kNewPinCancelled);
  }
  if (ui_presentation() == UIPresentation::kAutofill) {
    // Conditional UI requests are never cancelled, they restart silently.
    ResetEphemeralState();
    for (auto& observer : model_->observers) {
      observer.OnStartOver();
    }
    StartAutofillRequest();
    return;
  }

  if (is_request_complete()) {
    SetCurrentStep(Step::kClosed);
  }

  for (auto& observer : model_->observers) {
    observer.OnCancelRequest();
  }
}

void AuthenticatorRequestDialogController::OnRequestComplete() {
  if (ui_presentation() == UIPresentation::kAutofill) {
    auto* render_frame_host = GetRenderFrameHost();
    auto* web_contents =
        content::WebContents::FromRenderFrameHost(render_frame_host);
    if (web_contents && render_frame_host) {
      ChromeWebAuthnCredentialsDelegate* delegate =
          ChromeWebAuthnCredentialsDelegateFactory::GetFactory(web_contents)
              ->GetDelegateForFrame(render_frame_host);
      if (delegate) {
        delegate->NotifyWebAuthnRequestAborted();
      }
    }
  }
  SetCurrentStep(Step::kClosed);
}

void AuthenticatorRequestDialogController::OnResidentCredentialConfirmed() {
  DCHECK_EQ(model_->step(), Step::kResidentCredentialConfirmation);
  HideDialogAndDispatchToPlatformAuthenticator(AuthenticatorType::kWinNative);
}

void AuthenticatorRequestDialogController::OnHavePIN(std::u16string pin) {
  if (!pin_callback_) {
    // Protect against the view submitting a PIN more than once without
    // receiving a matching response first. |CollectPIN| is called again if
    // the user needs to be prompted for a retry.
    return;
  }
  std::move(pin_callback_).Run(pin);
}

void AuthenticatorRequestDialogController::EnclaveEnabledStatusChanged(
    EnclaveEnabledStatus status) {
  enclave_enabled_status_ = status;
}

void AuthenticatorRequestDialogController::OnAccountSelected(size_t index) {
  if (!selection_callback_) {
    // It's possible that the user could activate the dialog more than once
    // before the Webauthn request is completed and its torn down.
    return;
  }

  device::AuthenticatorGetAssertionResponse response =
      std::move(ephemeral_state_.responses_.at(index));
  model_->creds.clear();
  ephemeral_state_.responses_.clear();
  std::move(selection_callback_).Run(std::move(response));
}

void AuthenticatorRequestDialogController::OnAccountPreselectedIndex(
    size_t index) {
  OnAccountPreselected(model_->creds.at(index).cred_id);
}

void AuthenticatorRequestDialogController::OnBioEnrollmentDone() {
  std::move(bio_enrollment_callback_).Run();
}

void AuthenticatorRequestDialogController::OnUserConfirmedPriorityMechanism() {
  model_->mechanisms[*model_->priority_mechanism_index].callback.Run();
}

void AuthenticatorRequestDialogController::OnPasskeysChanged(
    const std::vector<webauthn::PasskeyModelChange>& changes) {}

void AuthenticatorRequestDialogController::OnPasskeyModelShuttingDown() {
  passkey_model_observation_.Reset();
}

void AuthenticatorRequestDialogController::OnPasskeyModelIsReady(
    bool is_ready) {}

void AuthenticatorRequestDialogController::PasskeyUpgradeSucceeded() {
  // Nothing to do. The WebAuthn request will be resolved automatically via the
  // request handler success callback. The PasskeyUpgradeRequestController shows
  // its own UI.
  CHECK_EQ(model_->step(), Step::kPasskeyUpgrade);
}

void AuthenticatorRequestDialogController::PasskeyUpgradeFailed() {
  CHECK_EQ(model_->step(), Step::kPasskeyUpgrade);
  CancelAuthenticatorRequest();
}

bool AuthenticatorRequestDialogController::is_request_complete() const {
  return model_->step() == Step::kTimedOut ||
         model_->step() == Step::kKeyNotRegistered ||
         model_->step() == Step::kKeyAlreadyRegistered ||
         model_->step() == Step::kMissingCapability ||
         model_->step() == Step::kErrorWindowsHelloNotEnabled ||
         model_->step() == Step::kErrorFetchingChallenge ||
         model_->step() == Step::kClosed;
}

void AuthenticatorRequestDialogController::StartFlow(
    TransportAvailabilityInfo transport_availability,
    PasswordCredentials passwords) {
  DCHECK(!started_);
  DCHECK_EQ(model_->step(), Step::kNotStarted);
  DCHECK_EQ(
      transport_availability.attestation_conveyance_preference.has_value(),
      transport_availability.request_type == FidoRequestType::kMakeCredential);

  started_ = true;
  transport_availability_ = std::move(transport_availability);
  passwords_ = std::move(passwords);
  // All recognised credentials that are "Chrome implemented" are from the
  // same source, i.e. a platform never has two Chrome implemented platform
  // authenticators.
  std::optional<AuthenticatorType> chrome_implemented_type;
  for (const auto& cred : transport_availability_.recognized_credentials) {
    if (IsChromeImplemented(cred.source)) {
      if (chrome_implemented_type.has_value()) {
        CHECK_EQ(*chrome_implemented_type, cred.source);
      } else {
        chrome_implemented_type = cred.source;
      }
    }
  }

  SortRecognizedCredentials();

#if BUILDFLAG(IS_MAC)
  RecordMacOsStartedHistogram();
#endif

  PopulateMechanisms();
  model_->priority_mechanism_index = IndexOfPriorityMechanism();

  switch (ui_presentation()) {
    case UIPresentation::kModal:
    case UIPresentation::kModalImmediate:
      StartGuidedFlowForMostLikelyTransportOrShowMechanismSelection();
      break;
    case UIPresentation::kAutofill:
      StartAutofillRequest();
      break;
    case UIPresentation::kPasskeyUpgrade:
      StartPasskeyUpgradeRequest();
      break;
    case UIPresentation::kDisabled:
      NOTREACHED();
  }
}

void AuthenticatorRequestDialogController::TransitionToModalWebAuthnRequest() {
  DCHECK_EQ(model_->step(), Step::kPasskeyAutofill);

  // Dispatch requests to any plugged in authenticators.
  for (auto& authenticator :
       ephemeral_state_.saved_authenticators_.authenticator_list()) {
    if (authenticator.transport != device::FidoTransportProtocol::kInternal) {
      DispatchRequestAsync(&authenticator);
    }
  }
  StartGuidedFlowForMostLikelyTransportOrShowMechanismSelection();
}

void AuthenticatorRequestDialogController::
    StartGuidedFlowForMostLikelyTransportOrShowMechanismSelection() {
  const bool enclave_will_do_uv = webauthn::GpmWillDoUserVerification(
      transport_availability_.user_verification_requirement,
      transport_availability_.platform_has_biometrics);
  constexpr bool kIsMac = BUILDFLAG(IS_MAC);

  MaybeStartChallengeFetch();

  if (pending_step_) {
    SetCurrentStep(*pending_step_);
    pending_step_.reset();
  } else if (model_->mechanisms.empty()) {
    if (transport_availability_.transport_list_did_include_internal) {
      SetCurrentStep(Step::kErrorNoPasskeys);
    } else {
      SetCurrentStep(Step::kErrorNoAvailableTransports);
    }
  } else if (model_->priority_mechanism_index) {
    Mechanism& mechanism =
        model_->mechanisms[*model_->priority_mechanism_index];
    const Mechanism::Credential* cred =
        std::get_if<Mechanism::Credential>(&mechanism.type);

    // If the authenticator will show its own confirmation then we don't want to
    // duplicate it.
    const bool authenticator_shows_own_confirmation =
        cred && (cred->value().source == AuthenticatorType::kICloudKeychain ||
                 cred->value().source == AuthenticatorType::kWinNative ||
                 // The enclave Touch ID prompts shows the credential details.
                 (cred->value().source == AuthenticatorType::kEnclave &&
                  enclave_will_do_uv && kIsMac &&
                  transport_availability_.platform_has_biometrics));

    if (cred != nullptr &&
        // Credentials on phones should never be triggered automatically.
        (cred->value().source == AuthenticatorType::kPhone ||
         // In the case of an empty allow list, the user should be able to see
         // the account that they're signing in with. So either
         // `kSelectPriorityMechanism` is used or else the authenticator shows
         // their own UI.
         (transport_availability_.has_empty_allow_list &&
          !authenticator_shows_own_confirmation) ||
         // Never auto-trigger macOS profile credentials without either a local
         // biometric or a UV requirement because, otherwise, there'll not be
         // *any* UI.
         (cred->value().source == AuthenticatorType::kTouchID &&
          !ProfileAuthenticatorWillDoUserVerification(
              transport_availability_.user_verification_requirement,
              transport_availability_.platform_has_biometrics)) ||
         // Never auto-trigger the enclave unless UV will be performed because,
         // otherwise, there'll not be any UI.
         (cred->value().source == AuthenticatorType::kEnclave &&
          !enclave_will_do_uv))) {
      SetCurrentStep(Step::kSelectPriorityMechanism);
    } else if (std::holds_alternative<Mechanism::Password>(mechanism.type)) {
      SetCurrentStep(Step::kSelectPriorityMechanism);
    } else {
      if (std::holds_alternative<Mechanism::Enclave>(mechanism.type)) {
        device::enclave::RecordEvent(
            device::enclave::Event::kMakeCredentialPriorityShown);
        enclave_was_priority_mechanism_ = true;
      } else {
        enclave_was_priority_mechanism_ = false;
      }
      mechanism.callback.Run();
    }
  } else {
    // If an allowlist was included and there are matches on a local
    // authenticator, jump to it. There are no mechanisms for these
    // authenticators so `priority_mechanism_index` cannot handle this.
    if (!transport_availability_.has_empty_allow_list) {
      if (transport_availability_.has_icloud_keychain_credential ==
              device::FidoRequestHandlerBase::RecognizedCredential::
                  kHasRecognizedCredential &&
          allow_icloud_keychain_) {
        ephemeral_state_.did_invoke_platform_despite_no_priority_mechanism_ =
            true;
        StartICloudKeychain();
        return;
      }
      if (transport_availability_.has_platform_authenticator_credential ==
          device::FidoRequestHandlerBase::RecognizedCredential::
              kHasRecognizedCredential) {
        ephemeral_state_.did_invoke_platform_despite_no_priority_mechanism_ =
            true;
        if (transport_availability_.has_win_native_api_authenticator) {
          StartWinNativeApi();
        } else {
          StartPlatformAuthenticatorFlow();
        }
        return;
      }

      // Don't jump to an enclave credential if we need to do reauth because the
      // OAuth token won't work. Also, don't jump to a phone credential either
      // because reauthenticating is probably a better option for the user.
      if (enclave_enabled_status_ !=
          EnclaveEnabledStatus::kEnabledAndReauthNeeded) {
        // If not doing UV, but the allowlist matches an enclave credential,
        // show UI to serve as user presence.
        if (!enclave_will_do_uv && transport_availability_.request_type ==
                                       FidoRequestType::kGetAssertion) {
          for (size_t i = 0; i < model_->mechanisms.size(); ++i) {
            const auto& type = model_->mechanisms[i].type;
            if (std::holds_alternative<Mechanism::Credential>(type) &&
                std::get<Mechanism::Credential>(type)->source ==
                    AuthenticatorType::kEnclave) {
              model_->priority_mechanism_index = i;
              SetCurrentStep(Step::kSelectPriorityMechanism);
              return;
            }
          }
        }
        if (transport_availability_.has_platform_authenticator_credential ==
            device::FidoRequestHandlerBase::RecognizedCredential::
                kNoRecognizedCredential) {
          // If there are no local matches but there are phone or enclave
          // passkeys, jump to the first one of them.
          for (auto& mechanism : model_->mechanisms) {
            const auto& type = mechanism.type;
            if (std::holds_alternative<Mechanism::Credential>(type)) {
              if (std::get<Mechanism::Credential>(type)->source ==
                  AuthenticatorType::kEnclave) {
                CHECK(enclave_will_do_uv);
                mechanism.callback.Run();
                return;
              }
            }
          }
        }
      }
    }
    // If a request includes credentials from Windows Hello only, jump directly
    // to Windows. There is little point Chrome UI as an extra step.
    if (transport_availability_.has_win_native_api_authenticator &&
        AreAllCredentialsWindowsHello(model_->mechanisms)) {
      ephemeral_state_.did_invoke_platform_despite_no_priority_mechanism_ =
          true;
      StartWinNativeApi();
      return;
    }
    if (!hints_.transport.has_value() ||
        transport_availability_.request_type !=
            FidoRequestType::kGetAssertion ||
        // If there were any matches, ignore a hint and show the user the list.
        std::ranges::any_of(model_->mechanisms,
                            [](const auto& mech) {
                              return std::get_if<Mechanism::Credential>(
                                  &mech.type);
                            }) ||
        !StartGuidedFlowForHint(*hints_.transport)) {
      SetCurrentStep(Step::kMechanismSelection);
    }
  }
}

bool AuthenticatorRequestDialogController::StartGuidedFlowForHint(
    AuthenticatorTransport transport) {
  // The RP has given a hint about the expected transport for a create() or
  // get() call.
  // See https://w3c.github.io/webauthn/#enum-hints
  if (transport == AuthenticatorTransport::kInternal &&
      enclave_enabled_status_ ==
          EnclaveEnabledStatus::kEnabledAndReauthNeeded) {
    // Go to the mechanism selection screen to give the user a chance to use
    // GPM.
    return false;
  }
  Profile* const profile =
      Profile::FromBrowserContext(GetRenderFrameHost()->GetBrowserContext())
          ->GetOriginalProfile();
  bool can_default_to_enclave = CanDefaultToEnclave(profile);

  const auto mech_it = std::ranges::find_if(
      model_->mechanisms,
      [transport, can_default_to_enclave](const auto& mech) {
        if (std::holds_alternative<Mechanism::Enclave>(mech.type) &&
            !can_default_to_enclave) {
          return false;
        }
        return MechanismMatchesHint(mech.type, transport);
      });

  if (mech_it != model_->mechanisms.end()) {
    if (transport == AuthenticatorTransport::kInternal) {
      ephemeral_state_.did_invoke_platform_despite_no_priority_mechanism_ =
          true;
    }
    mech_it->callback.Run();
    return true;
  }

  return false;
}

void AuthenticatorRequestDialogController::
    HideDialogAndDispatchToPlatformAuthenticator(
        std::optional<AuthenticatorType> type) {
  SetCurrentStep(Step::kPlatformAuthenticator);

  std::vector<AuthenticatorReference>& authenticators =
      ephemeral_state_.saved_authenticators_.authenticator_list();
#if BUILDFLAG(IS_WIN)
  // The Windows-native UI already handles retrying so we do not offer a second
  // level of retry in that case.
  if (type && *type != AuthenticatorType::kEnclave) {
    model_->offer_try_again_in_ui = false;
  }
#elif BUILDFLAG(IS_MAC)
  // If there are multiple platform authenticators, one of them is the default.
  if (!type.has_value()) {
    if (std::ranges::any_of(
            authenticators, [](const AuthenticatorReference& ref) {
              return ref.type == AuthenticatorType::kOther &&
                     ref.transport == device::FidoTransportProtocol::kInternal;
            })) {
      type = AuthenticatorType::kOther;
    }
  }

  if (!type.has_value()) {
    type = AuthenticatorType::kTouchID;
  }
#endif

  auto platform_authenticator_it = std::ranges::find_if(
      authenticators, [type](const AuthenticatorReference& ref) -> bool {
        if (type && *type == AuthenticatorType::kEnclave) {
          return ref.type == *type;
        }
        return ref.transport == device::FidoTransportProtocol::kInternal &&
               (!type || ref.type == *type);
      });

  if (platform_authenticator_it == authenticators.end()) {
    return;
  }

  ephemeral_state_.dispatched_platform_authenticator_type_ =
      platform_authenticator_it->type;
  if (platform_authenticator_it->type == AuthenticatorType::kICloudKeychain) {
    webauthn::user_actions::RecordICloudShown(
        transport_availability_.request_type);
  } else if (platform_authenticator_it->type == AuthenticatorType::kTouchID) {
    webauthn::user_actions::RecordChromeProfileAuthenticatorShown(
        transport_availability_.request_type);
  } else if (platform_authenticator_it->type == AuthenticatorType::kWinNative) {
    webauthn::user_actions::RecordWindowsHelloShown(
        transport_availability_.request_type);
  }

  DispatchRequestAsync(&*platform_authenticator_it);
}

void AuthenticatorRequestDialogController::OnCableEvent(
    device::cablev2::Event event) {
  switch (event) {
    case device::cablev2::Event::kPhoneConnected:
    case device::cablev2::Event::kBLEAdvertReceived:
      if (model_->step() != Step::kCableV2Connecting) {
        SetCurrentStep(Step::kCableV2Connecting);
        cable_connecting_sheet_timer_.Start(
            FROM_HERE, base::Milliseconds(1250),
            base::BindOnce(&AuthenticatorRequestDialogController::
                               OnCableConnectingTimerComplete,
                           weak_factory_.GetWeakPtr()));
      }
      break;
    case device::cablev2::Event::kReady:
      if (cable_connecting_sheet_timer_.IsRunning()) {
        cable_connecting_ready_to_advance_ = true;
      } else {
        SetCurrentStep(Step::kCableV2Connected);
      }
      break;
  }
}

void AuthenticatorRequestDialogController::OnCableConnectingTimerComplete() {
  if (cable_connecting_ready_to_advance_ &&
      model_->step() == Step::kCableV2Connecting) {
    SetCurrentStep(Step::kCableV2Connected);
  }
}

void AuthenticatorRequestDialogController::EnsureBleAdapterIsPoweredAndContinue(
    base::OnceClosure action) {
  after_ble_adapter_powered_ = std::move(action);
  if (transport_availability_.ble_status ==
      BleStatus::kPendingPermissionRequest) {
    // Trigger requesting Bluetooth permissions on macOS.
    model_->ui_disabled_ = true;
    model_->OnSheetModelChanged();
    request_ble_permission_callback_.Run(
        base::BindOnce(&AuthenticatorRequestDialogController::OnBleStatusKnown,
                       weak_factory_.GetWeakPtr()));
    return;
  }
  OnBleStatusKnown(transport_availability_.ble_status);
}

void AuthenticatorRequestDialogController::OnBleStatusKnown(
    BleStatus ble_status) {
  if (!after_ble_adapter_powered_) {
    // Drop the callback if there is no action pending after the adapter is
    // powered. This could happen e.g. if the
    // EnsureBleAdapterIsPoweredAndContinue was called twice before
    // OnBleStatusKnown had a chance to resolve.
    FIDO_LOG(ERROR) << "Ignoring BLE status: no action pending.";
    return;
  }
  model_->ui_disabled_ = false;
  transport_availability_.ble_status = ble_status;
  model_->ble_adapter_is_powered =
      transport_availability_.ble_status ==
      device::FidoRequestHandlerBase::BleStatus::kOn;
  switch (transport_availability_.ble_status) {
    case BleStatus::kOn:
      std::move(after_ble_adapter_powered_).Run();
      return;
    case BleStatus::kOff:
      if (transport_availability_.can_power_on_ble_adapter) {
        SetCurrentStep(Step::kBlePowerOnAutomatic);
      } else {
        SetCurrentStep(Step::kBlePowerOnManual);
      }
      return;
    case BleStatus::kPermissionDenied:
      // |step| is not saved because macOS asks the user to restart Chrome
      // after permission has been granted. So the user will end up retrying
      // the whole WebAuthn request in the new process.
      SetCurrentStep(Step::kBlePermissionMac);
      return;
    case BleStatus::kPendingPermissionRequest:
      // This should have been handled by EnsureBleAdapterIsPoweredAndContinue.
      NOTREACHED();
  }
}

void AuthenticatorRequestDialogController::TryUsbDevice() {
  DCHECK_EQ(model_->step(), Step::kUsbInsertAndActivate);
}

void AuthenticatorRequestDialogController::StartPlatformAuthenticatorFlow() {
  if (transport_availability_.request_type == FidoRequestType::kGetAssertion) {
    switch (transport_availability_.has_platform_authenticator_credential) {
      case device::FidoRequestHandlerBase::RecognizedCredential::kUnknown:
        NOTREACHED();
      case device::FidoRequestHandlerBase::RecognizedCredential::
          kNoRecognizedCredential:
        // Never try the platform authenticator if the request is known in
        // advance to fail. Proceed to a special error screen instead.
        SetCurrentStep(Step::kErrorInternalUnrecognized);
        return;
      case device::FidoRequestHandlerBase::RecognizedCredential::
          kHasRecognizedCredential:
        break;
    }

    // If the platform authenticator reports known credentials, show them in the
    // UI. It is possible for the platform authenticator to report
    // `kHasRecognizedCredential` without reporting any metadata (e.g. Chrome
    // OS) but `recognized_credentials` could include other credentials. So we
    // need to filter to check that metadata for a Chrome-implemented platform
    // authenticator is really present.
    std::vector<device::DiscoverableCredentialMetadata> platform_credentials;
    std::ranges::copy_if(
        transport_availability_.recognized_credentials,
        std::back_inserter(platform_credentials),
        [](const auto& cred) { return IsChromeImplemented(cred.source); });

    if (!platform_credentials.empty()) {
      if (transport_availability_.has_empty_allow_list) {
        // For discoverable credential requests, show an account picker.
        model_->creds = std::move(platform_credentials);
        SetCurrentStep(Step::kPreSelectAccount);
      } else {
        // For requests with an allow list, pre-select a random credential.
        model_->creds = {platform_credentials.front()};
        if (ProfileAuthenticatorWillDoUserVerification(
                transport_availability_.user_verification_requirement,
                transport_availability_.platform_has_biometrics)) {
          // If it's not preferable to complete the request by clicking
          // "Continue" then don't show the account selection sheet.
          HideDialogAndDispatchToPlatformAuthenticator();
        } else {
          // Otherwise show the chosen credential to the user. For platform
          // authenticators with optional UV (e.g. Touch ID), this step
          // essentially acts as the user presence check.
          SetCurrentStep(Step::kPreSelectAccount);
        }
      }
      return;
    }
  }

  if (transport_availability_.request_type ==
      FidoRequestType::kMakeCredential) {
    if (kShowCreatePlatformPasskeyStep) {
      SetCurrentStep(Step::kCreatePasskey);
      return;
    }

    if (model_->is_off_the_record) {
      // Step::kCreatePasskey incorporates an incognito warning if
      // applicable, so the OTR interstitial step only needs to show in the
      // "old" UI.
      after_off_the_record_interstitial_ =
          base::BindOnce(&AuthenticatorRequestDialogController::
                             HideDialogAndDispatchToPlatformAuthenticator,
                         weak_factory_.GetWeakPtr(), std::nullopt);
      SetCurrentStep(Step::kOffTheRecordInterstitial);
      return;
    }
  }

  HideDialogAndDispatchToPlatformAuthenticator();
}

void AuthenticatorRequestDialogController::OnRequestTimeout() {
  // The request may time out while the UI shows a different error.
  if (!is_request_complete()) {
    SetCurrentStep(Step::kTimedOut);
  }
}

void AuthenticatorRequestDialogController::OnActivatedKeyNotRegistered() {
  DCHECK(!is_request_complete());
  SetCurrentStep(Step::kKeyNotRegistered);
}

void AuthenticatorRequestDialogController::OnActivatedKeyAlreadyRegistered() {
  DCHECK(!is_request_complete());
  SetCurrentStep(Step::kKeyAlreadyRegistered);
}

void AuthenticatorRequestDialogController::OnSoftPINBlock() {
  SetCurrentStep(Step::kClientPinErrorSoftBlock);
}

void AuthenticatorRequestDialogController::OnHardPINBlock() {
  SetCurrentStep(Step::kClientPinErrorHardBlock);
}

void AuthenticatorRequestDialogController::
    OnAuthenticatorRemovedDuringPINEntry() {
  SetCurrentStep(Step::kClientPinErrorAuthenticatorRemoved);
}

void AuthenticatorRequestDialogController::
    OnAuthenticatorMissingResidentKeys() {
  SetCurrentStep(Step::kMissingCapability);
}

void AuthenticatorRequestDialogController::
    OnAuthenticatorMissingUserVerification() {
  SetCurrentStep(Step::kMissingCapability);
}

void AuthenticatorRequestDialogController::OnAuthenticatorMissingLargeBlob() {
  // TODO(nsatragno): on Windows we should have a more accurate message if large
  // blob is missing.
  SetCurrentStep(Step::kMissingCapability);
}

void AuthenticatorRequestDialogController::OnNoCommonAlgorithms() {
  SetCurrentStep(Step::kMissingCapability);
}

void AuthenticatorRequestDialogController::OnAuthenticatorStorageFull() {
  SetCurrentStep(Step::kStorageFull);
}

void AuthenticatorRequestDialogController::OnUserConsentDenied() {
  if (ui_presentation() == UIPresentation::kAutofill) {
    // Do not show a page-modal retry error sheet if the user cancelled out of
    // their platform authenticator during a conditional UI request.
    // Instead, retry silently.
    CancelAuthenticatorRequest();
    return;
  }

  if (ephemeral_state_.dispatched_platform_authenticator_type_ ==
      AuthenticatorType::kICloudKeychain) {
    webauthn::user_actions::RecordICloudCancelled();
    // If we dispatched automatically to iCloud Keychain and the
    // user clicked cancel, give them the option to try something else.
    bool did_trigger_automatically =
        ephemeral_state_.did_invoke_platform_despite_no_priority_mechanism_;
    if (!did_trigger_automatically &&
        model_->priority_mechanism_index.has_value()) {
      const auto& priority_type =
          model_->mechanisms[*model_->priority_mechanism_index].type;
      if (std::holds_alternative<Mechanism::Credential>(priority_type)) {
        const Mechanism::CredentialInfo* cred_info =
            &std::get<Mechanism::Credential>(priority_type).value();
        if (cred_info->source == AuthenticatorType::kICloudKeychain) {
          did_trigger_automatically = true;
        }
      } else if (std::holds_alternative<Mechanism::ICloudKeychain>(
                     priority_type)) {
        did_trigger_automatically = true;
      }
    }

    if (did_trigger_automatically && model_->mechanisms.size() > 1) {
      StartOver();
    } else {
      // Otherwise, respect the "Cancel" button in macOS UI as if it were our
      // own.
      CancelAuthenticatorRequest();
    }
    return;
  }
  if (ephemeral_state_.dispatched_platform_authenticator_type_ ==
      AuthenticatorType::kTouchID) {
    webauthn::user_actions::RecordChromeProfileCancelled();
    if (ui_presentation() == UIPresentation::kModalImmediate) {
      // On immediate mode there's no need to show the error sheet where the
      // user can retry. Instead fail early and let the relying party handle the
      // error.
      CancelAuthenticatorRequest();
      return;
    }
  }
  SetCurrentStep(Step::kErrorInternalUnrecognized);
}

bool AuthenticatorRequestDialogController::OnWinUserCancelled() {
#if BUILDFLAG(IS_WIN)
  if (ui_presentation() == UIPresentation::kAutofill) {
    // Do not show a page-modal retry error sheet if the user cancelled out of
    // their platform authenticator during a conditional UI request.
    // Instead, retry silently.
    CancelAuthenticatorRequest();
    return true;
  }

  if (ephemeral_state_.dispatched_platform_authenticator_type_ ==
      AuthenticatorType::kWinNative) {
    webauthn::user_actions::RecordWindowsHelloCancelled();
  }
  // If the native Windows API was triggered immediately (i.e. before any Chrome
  // dialog) then start the request over (once) if the user cancels the Windows
  // UI and there are other options in Chrome's UI.
  bool enclave_is_option =
      std::ranges::any_of(model_->mechanisms, [](const Mechanism& m) {
        return std::holds_alternative<Mechanism::Enclave>(m.type);
      });
  bool phone_is_option =
      !WebAuthnApiSupportsHybrid() &&
      std::ranges::any_of(model_->mechanisms, [](const Mechanism& m) -> bool {
        return std::holds_alternative<Mechanism::Hybrid>(m.type);
      });
  bool have_other_option = enclave_is_option || phone_is_option;
  bool windows_was_priority =
      ephemeral_state_.did_invoke_platform_despite_no_priority_mechanism_ ||
      (model_->priority_mechanism_index &&
       std::holds_alternative<Mechanism::WindowsAPI>(
           model_->mechanisms[*model_->priority_mechanism_index].type));
  if (have_other_option && windows_was_priority) {
    StartOver();
    return true;
  }
#endif

  return false;
}

bool AuthenticatorRequestDialogController::OnHybridTransportError() {
  SetCurrentStep(Step::kCableV2Error);
  return true;
}

bool AuthenticatorRequestDialogController::OnEnclaveError() {
  SetCurrentStep(Step::kGPMError);
  return true;
}

bool AuthenticatorRequestDialogController::OnNoPasskeys() {
  SetCurrentStep(Step::kErrorNoPasskeys);
  return true;
}

void AuthenticatorRequestDialogController::OnChallengeUrlFailure() {
  if (!is_request_complete()) {
    SetCurrentStep(Step::kErrorFetchingChallenge);
  }
}

void AuthenticatorRequestDialogController::BluetoothAdapterStatusChanged(
    BleStatus ble_status) {
  transport_availability_.ble_status = ble_status;
  model_->ble_adapter_is_powered = ble_status == BleStatus::kOn;
  model_->OnBluetoothPoweredStateChanged();

  // For the manual flow, the user has to click the "next" button explicitly.
  if (model_->step() == Step::kBlePowerOnAutomatic) {
    ContinueWithFlowAfterBleAdapterPowered();
  }
}

void AuthenticatorRequestDialogController::SetRequestCallback(
    RequestCallback request_callback) {
  request_callback_ = request_callback;
}

void AuthenticatorRequestDialogController::SetAccountPreselectedCallback(
    content::AuthenticatorRequestClientDelegate::AccountPreselectedCallback
        callback) {
  account_preselected_callback_ = callback;
}

void AuthenticatorRequestDialogController::SetBluetoothAdapterPowerOnCallback(
    base::RepeatingClosure bluetooth_adapter_power_on_callback) {
  bluetooth_adapter_power_on_callback_ = bluetooth_adapter_power_on_callback;
}

void AuthenticatorRequestDialogController::SetRequestBlePermissionCallback(
    BlePermissionCallback callback) {
  request_ble_permission_callback_ = std::move(callback);
}

void AuthenticatorRequestDialogController::OnRetryUserVerification(
    int attempts) {
  model_->uv_attempts = attempts;
  SetCurrentStep(Step::kRetryInternalUserVerification);
}

void AuthenticatorRequestDialogController::AddAuthenticator(
    const device::FidoAuthenticator& authenticator) {
  // Only the webauthn.dll authenticator omits a transport completely. This
  // makes sense given how it works, but here it is treated as a platform
  // authenticator and so given a `kInternal` transport.
  DCHECK(authenticator.AuthenticatorTransport() ||
         authenticator.GetType() == AuthenticatorType::kWinNative);
  const AuthenticatorTransport transport =
      authenticator.AuthenticatorTransport().value_or(
          AuthenticatorTransport::kInternal);

  AuthenticatorReference authenticator_reference(
      authenticator.GetId(), transport, authenticator.GetType());

  ephemeral_state_.saved_authenticators_.AddAuthenticator(
      std::move(authenticator_reference));
}

void AuthenticatorRequestDialogController::RemoveAuthenticator(
    std::string_view authenticator_id) {
  ephemeral_state_.saved_authenticators_.RemoveAuthenticator(authenticator_id);
}

// SelectAccount is called to trigger an account selection dialog.
void AuthenticatorRequestDialogController::SelectAccount(
    std::vector<device::AuthenticatorGetAssertionResponse> responses,
    base::OnceCallback<void(device::AuthenticatorGetAssertionResponse)>
        callback) {
  ephemeral_state_.responses_ = std::move(responses);
  model_->creds = {};
  for (const auto& response : ephemeral_state_.responses_) {
    model_->creds.emplace_back(AuthenticatorType::kOther,
                               model_->relying_party_id,
                               response.credential->id, *response.user_entity,
                               /*provider_name=*/std::nullopt);
  }
  selection_callback_ = std::move(callback);
  SetCurrentStep(Step::kSelectAccount);
}

AuthenticatorType AuthenticatorRequestDialogController::OnAccountPreselected(
    const std::vector<uint8_t> credential_id) {
  // User selected one of the platform authenticator credentials enumerated in
  // Conditional or regular modal UI prior to collecting user verification.
  // Run `account_preselected_callback_` to narrow the request to the selected
  // credential and dispatch to the platform authenticator.
  const auto cred =
      std::ranges::find_if(transport_availability_.recognized_credentials,
                           [&credential_id](const auto& cred) {
                             return cred.cred_id == credential_id;
                           });
  CHECK(cred != transport_availability_.recognized_credentials.end())
      << "OnAccountPreselected() called with unknown credential_id "
      << base::HexEncode(credential_id);
  const AuthenticatorType source = cred->source;
  DCHECK(account_preselected_callback_);
  account_preselected_callback_.Run(*cred);
  model_->preselected_cred = *cred;

  MaybeStartChallengeFetch();

  // `source` should not be `kPhone` here.
  if (source != AuthenticatorType::kEnclave) {
    HideDialogAndDispatchToPlatformAuthenticator(source);
    return source;
  }

  model_->OnGPMPasskeySelected(credential_id);
  return source;
}

void AuthenticatorRequestDialogController::SetSelectedAuthenticatorForTesting(
    AuthenticatorReference test_authenticator) {
  ephemeral_state_.saved_authenticators_.AddAuthenticator(
      std::move(test_authenticator));
}

void AuthenticatorRequestDialogController::StartTransportFlowForTesting(
    AuthenticatorTransport transport) {
  StartGuidedFlowForTransport(transport);
}

void AuthenticatorRequestDialogController::SetCurrentStepForTesting(Step step) {
  SetCurrentStep(step);
}

void AuthenticatorRequestDialogController::CollectPIN(
    device::pin::PINEntryReason reason,
    device::pin::PINEntryError error,
    uint32_t min_pin_length,
    int attempts,
    base::OnceCallback<void(std::u16string)> provide_pin_cb) {
  pin_callback_ = std::move(provide_pin_cb);
  model_->min_pin_length = min_pin_length;
  model_->pin_error = error;
  switch (reason) {
    case device::pin::PINEntryReason::kChallenge:
      model_->pin_attempts = attempts;
      SetCurrentStep(Step::kClientPinEntry);
      return;
    case device::pin::PINEntryReason::kChange:
      SetCurrentStep(Step::kClientPinChange);
      return;
    case device::pin::PINEntryReason::kSet:
      SetCurrentStep(Step::kClientPinSetup);
      return;
  }
}

void AuthenticatorRequestDialogController::FinishCollectToken() {
  SetCurrentStep(Step::kClientPinTapAgain);
}

void AuthenticatorRequestDialogController::StartInlineBioEnrollment(
    base::OnceClosure next_callback) {
  model_->max_bio_samples = std::nullopt;
  model_->bio_samples_remaining = std::nullopt;
  bio_enrollment_callback_ = std::move(next_callback);
  SetCurrentStep(Step::kInlineBioEnrollment);
}

void AuthenticatorRequestDialogController::OnSampleCollected(
    int bio_samples_remaining) {
  DCHECK(model_->step() == Step::kInlineBioEnrollment);

  model_->bio_samples_remaining = bio_samples_remaining;
  if (!model_->max_bio_samples) {
    model_->max_bio_samples = bio_samples_remaining + 1;
  }
  model_->OnSheetModelChanged();
}

void AuthenticatorRequestDialogController::set_cable_transport_info(
    std::optional<bool> extension_is_v2,
    const std::optional<std::string>& cable_qr_string) {
  if (extension_is_v2.has_value()) {
    if (*extension_is_v2) {
      model_->cable_ui_type =
          AuthenticatorRequestDialogModel::CableUIType::CABLE_V2_SERVER_LINK;
    } else {
      model_->cable_ui_type =
          AuthenticatorRequestDialogModel::CableUIType::CABLE_V1;
    }
  } else {
    model_->cable_ui_type =
        AuthenticatorRequestDialogModel::CableUIType::CABLE_V2_2ND_FACTOR;
  }

  model_->cable_qr_string = cable_qr_string;
}

void AuthenticatorRequestDialogController::set_allow_icloud_keychain(
    bool is_allowed) {
  allow_icloud_keychain_ = is_allowed;
}

void AuthenticatorRequestDialogController::set_should_create_in_icloud_keychain(
    bool is_enabled) {
  should_create_in_icloud_keychain_ = is_enabled;
}

#if BUILDFLAG(IS_MAC)

// This enum is used in a histogram. Never change assigned values and only add
// new entries at the end.
enum class MacOsHistogramValues {
  kStartedCreateForProfileAuthenticatorICloudDriveEnabled = 0,
  kStartedCreateForProfileAuthenticatorICloudDriveDisabled = 1,
  kStartedCreateForICloudKeychainICloudDriveEnabled = 2,
  kStartedCreateForICloudKeychainICloudDriveDisabled = 3,

  kSuccessfulCreateForProfileAuthenticatorICloudDriveEnabled = 4,
  kSuccessfulCreateForProfileAuthenticatorICloudDriveDisabled = 5,
  kSuccessfulCreateForICloudKeychainICloudDriveEnabled = 6,
  kSuccessfulCreateForICloudKeychainICloudDriveDisabled = 7,

  kStartedGetOnlyProfileAuthenticatorRecognised = 8,
  kStartedGetOnlyICloudKeychainRecognised = 9,
  kStartedGetBothRecognised = 10,

  kSuccessfulGetFromProfileAuthenticator = 11,
  kSuccessfulGetFromICloudKeychain = 12,
  kMaxValue = kSuccessfulGetFromICloudKeychain,
};

void AuthenticatorRequestDialogController::RecordMacOsStartedHistogram() {
  if (is_non_webauthn_request_ || model_->relying_party_id == "google.com") {
    return;
  }

  std::optional<MacOsHistogramValues> v;
  if (transport_availability_.request_type ==
          FidoRequestType::kMakeCredential &&
      transport_availability_.make_credential_attachment.has_value() &&
      *transport_availability_.make_credential_attachment !=
          device::AuthenticatorAttachment::kCrossPlatform) {
    if (should_create_in_icloud_keychain_) {
      v = has_icloud_drive_enabled_
              ? MacOsHistogramValues::
                    kStartedCreateForICloudKeychainICloudDriveEnabled
              : MacOsHistogramValues::
                    kStartedCreateForICloudKeychainICloudDriveDisabled;
    } else {
      v = has_icloud_drive_enabled_
              ? MacOsHistogramValues::
                    kStartedCreateForProfileAuthenticatorICloudDriveEnabled
              : MacOsHistogramValues::
                    kStartedCreateForProfileAuthenticatorICloudDriveDisabled;
    }
  } else if (transport_availability_.request_type ==
                 device::FidoRequestType::kGetAssertion &&
             IsModalRequest(ui_presentation())) {
    const bool profile =
        transport_availability_.has_platform_authenticator_credential ==
        device::FidoRequestHandlerBase::RecognizedCredential::
            kHasRecognizedCredential;
    const bool icloud =
        transport_availability_.has_icloud_keychain_credential ==
        device::FidoRequestHandlerBase::RecognizedCredential::
            kHasRecognizedCredential;
    if (profile && !icloud) {
      v = MacOsHistogramValues::kStartedGetOnlyProfileAuthenticatorRecognised;
    } else if (icloud && !profile) {
      v = MacOsHistogramValues::kStartedGetOnlyICloudKeychainRecognised;
    } else if (icloud && profile) {
      v = MacOsHistogramValues::kStartedGetBothRecognised;
    }
  }

  if (v) {
    base::UmaHistogramEnumeration(
        "WebAuthentication.MacOS.PlatformAuthenticatorAction", *v);
    did_record_macos_start_histogram_ = true;
  }
}

void AuthenticatorRequestDialogController::RecordMacOsSuccessHistogram(
    FidoRequestType request_type,
    AuthenticatorType authenticator_type) {
  if (!did_record_macos_start_histogram_) {
    return;
  }

  std::optional<MacOsHistogramValues> v;

  if (transport_availability_.request_type ==
      FidoRequestType::kMakeCredential) {
    if (authenticator_type == AuthenticatorType::kTouchID) {
      v = has_icloud_drive_enabled_
              ? MacOsHistogramValues::
                    kSuccessfulCreateForProfileAuthenticatorICloudDriveEnabled
              : MacOsHistogramValues::
                    kSuccessfulCreateForProfileAuthenticatorICloudDriveDisabled;
    } else if (authenticator_type == AuthenticatorType::kICloudKeychain) {
      v = has_icloud_drive_enabled_
              ? MacOsHistogramValues::
                    kSuccessfulCreateForICloudKeychainICloudDriveEnabled
              : MacOsHistogramValues::
                    kSuccessfulCreateForICloudKeychainICloudDriveDisabled;
    }
  } else {
    if (authenticator_type == AuthenticatorType::kTouchID) {
      v = MacOsHistogramValues::kSuccessfulGetFromProfileAuthenticator;
    } else if (authenticator_type == AuthenticatorType::kICloudKeychain) {
      v = MacOsHistogramValues::kSuccessfulGetFromICloudKeychain;
    }
  }

  if (v) {
    base::UmaHistogramEnumeration(
        "WebAuthentication.MacOS.PlatformAuthenticatorAction", *v);
  }
}

void AuthenticatorRequestDialogController::set_has_icloud_drive_enabled(
    bool is_enabled) {
  has_icloud_drive_enabled_ = is_enabled;
}

#endif

void AuthenticatorRequestDialogController::SetCredentialTypes(int types) {
  credential_types_ = types;
}

content::AuthenticatorRequestClientDelegate::UIPresentation
AuthenticatorRequestDialogController::ui_presentation() const {
  return model_->ui_presentation;
}

void AuthenticatorRequestDialogController::SetUIPresentation(
    UIPresentation modality) {
  model_->set_ui_presentation(modality);
}

void AuthenticatorRequestDialogController::ProvideChallengeUrl(
    const GURL& url,
    base::OnceCallback<void(std::optional<base::span<const uint8_t>>)>
        callback) {
  CHECK(url.is_valid());
  challenge_url_ = url;
  challenge_callback_ = std::move(callback);

  // Conditional requests don't initiate a challenge fetch unless and until the
  // user triggers it, but modal requests always perform the fetch so it can
  // be started immediately.
  if (IsModalRequest(ui_presentation())) {
    MaybeStartChallengeFetch();
  }
}

void AuthenticatorRequestDialogController::InitializeEnclaveRequestCallback(
    device::FidoDiscoveryFactory* discovery_factory) {
  CHECK(!enclave_request_callback_);

  using EnclaveEventStream = device::FidoDiscoveryBase::EventStream<
      std::unique_ptr<device::enclave::CredentialRequest>>;
  std::unique_ptr<EnclaveEventStream> event_stream;
  std::tie(enclave_request_callback_, event_stream) = EnclaveEventStream::New();
  discovery_factory->set_enclave_ui_request_stream(std::move(event_stream));
}

void AuthenticatorRequestDialogController::MaybeStartChallengeFetch() {
  if (!challenge_callback_) {
    return;
  }

  auto challenge_or_error = GetChallengeUrlFetcher()->GetChallenge();
  if (!challenge_or_error.has_value() &&
      challenge_or_error.error() ==
          ChallengeUrlFetcher::ChallengeNotAvailableReason::kNotRequested) {
    GetChallengeUrlFetcher()->FetchUrl(
        challenge_url_,
        base::BindOnce(
            &AuthenticatorRequestDialogController::OnChallengeFetched,
            weak_factory_.GetWeakPtr()));
  }
}

void AuthenticatorRequestDialogController::OnChallengeFetched() {
  auto challenge_or_error = GetChallengeUrlFetcher()->GetChallenge();

  if (challenge_or_error.has_value()) {
    std::move(challenge_callback_).Run(challenge_or_error.value());
    return;
  }

  CHECK_EQ(challenge_or_error.error(),
           ChallengeUrlFetcher::ChallengeNotAvailableReason::
               kErrorFetchingChallenge);

  std::move(challenge_callback_).Run(std::nullopt);
}

ChallengeUrlFetcher*
AuthenticatorRequestDialogController::GetChallengeUrlFetcher() {
  if (!challenge_url_fetcher_) {
    challenge_url_fetcher_ = std::make_unique<ChallengeUrlFetcher>(
        Profile::FromBrowserContext(GetRenderFrameHost()->GetBrowserContext())
            ->GetDefaultStoragePartition()
            ->GetURLLoaderFactoryForBrowserProcess());
  }
  return challenge_url_fetcher_.get();
}

base::WeakPtr<AuthenticatorRequestDialogController>
AuthenticatorRequestDialogController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void AuthenticatorRequestDialogController::SetCurrentStep(Step step) {
  if (!started_) {
    // Dialog isn't showing yet. Remember to show this step when it appears.
    pending_step_ = step;
    return;
  }

  // Reset state related to automatically advancing the state.
  cable_connecting_sheet_timer_.Stop();
  cable_connecting_ready_to_advance_ = false;

  model_->SetStep(step);
}

void AuthenticatorRequestDialogController::StartGuidedFlowForTransport(
    AuthenticatorTransport transport) {
  DCHECK(model_->step() == Step::kMechanismSelection ||
         model_->step() == Step::kUsbInsertAndActivate ||
         model_->step() == Step::kCableActivate ||
         model_->step() == Step::kPasskeyAutofill ||
         model_->step() == Step::kCreatePasskey ||
         model_->step() == Step::kPreSelectAccount ||
         model_->step() == Step::kSelectPriorityMechanism ||
         model_->step() == Step::kSelectAccount ||
         model_->step() == Step::kNotStarted);
  switch (transport) {
    case AuthenticatorTransport::kUsbHumanInterfaceDevice:
      SetCurrentStep(Step::kUsbInsertAndActivate);
      break;
    case AuthenticatorTransport::kInternal:
      StartPlatformAuthenticatorFlow();
      break;
    case AuthenticatorTransport::kHybrid:
      EnsureBleAdapterIsPoweredAndContinue(
          base::BindOnce(&AuthenticatorRequestDialogController::SetCurrentStep,
                         weak_factory_.GetWeakPtr(), Step::kCableActivate));
      break;
    default:
      break;
  }
}

void AuthenticatorRequestDialogController::StartHybridFlow() {
  EnsureBleAdapterIsPoweredAndContinue(
      base::BindOnce(&AuthenticatorRequestDialogController::SetCurrentStep,
                     weak_factory_.GetWeakPtr(), Step::kCableV2QRCode));
}

void AuthenticatorRequestDialogController::StartWinNativeApi() {
  DCHECK(transport_availability_.has_win_native_api_authenticator);
  if (transport_availability_.request_is_internal_only &&
      !transport_availability_.win_is_uvpaa) {
    model_->offer_try_again_in_ui = false;
    SetCurrentStep(Step::kErrorWindowsHelloNotEnabled);
    return;
  }

  if (model_->resident_key_requirement !=
          device::ResidentKeyRequirement::kDiscouraged &&
      !transport_availability_.win_native_ui_shows_resident_credential_notice) {
    SetCurrentStep(Step::kResidentCredentialConfirmation);
  } else {
    HideDialogAndDispatchToPlatformAuthenticator(AuthenticatorType::kWinNative);
  }
}

void AuthenticatorRequestDialogController::StartICloudKeychain() {
  DCHECK(transport_availability_.has_icloud_keychain);
  if (transport_availability_.has_icloud_keychain_credential ==
          device::FidoRequestHandlerBase::RecognizedCredential::
              kHasRecognizedCredential &&
      !transport_availability_.has_empty_allow_list) {
    // For requests with an allow list, pre-select a random credential.
    const device::DiscoverableCredentialMetadata* selected = nullptr;
    for (const auto& cred : transport_availability_.recognized_credentials) {
      if (cred.source == AuthenticatorType::kICloudKeychain) {
        selected = &cred;
        break;
      }
    }
    account_preselected_callback_.Run(*selected);
  }

  HideDialogAndDispatchToPlatformAuthenticator(
      AuthenticatorType::kICloudKeychain);
}

void AuthenticatorRequestDialogController::StartEnclave() {
  model_->OnGPMSelected();
}

void AuthenticatorRequestDialogController::ReauthForSyncRestore() {
  signin_ui_util::ShowReauthForPrimaryAccountWithAuthError(
    Profile::FromBrowserContext(GetRenderFrameHost()->GetBrowserContext())
        ->GetOriginalProfile(),
    signin_metrics::AccessPoint::kWebauthnModalDialog);
  CancelAuthenticatorRequest();
}

void AuthenticatorRequestDialogController::StartAutofillRequest() {
  model_->creds = transport_availability_.recognized_credentials;

  auto* render_frame_host = GetRenderFrameHost();
  auto* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  std::vector<password_manager::PasskeyCredential> credentials;
  std::optional<std::u16string> priority_phone_name;
  for (const auto& credential : model_->creds) {
    if (credential.source == AuthenticatorType::kEnclave &&
        enclave_enabled_status_ != EnclaveEnabledStatus::kEnabled) {
      continue;
    }
    password_manager::PasskeyCredential& passkey = credentials.emplace_back(
        ToPasswordManagerSource(credential.source),
        password_manager::PasskeyCredential::RpId(credential.rp_id),
        password_manager::PasskeyCredential::CredentialId(credential.cred_id),
        password_manager::PasskeyCredential::UserId(credential.user.id),
        password_manager::PasskeyCredential::Username(
            credential.user.name.value_or("")),
        password_manager::PasskeyCredential::DisplayName(
            credential.user.display_name.value_or("")));
    if (credential.provider_name) {
      passkey.SetAuthenticatorLabel(l10n_util::GetStringFUTF16(
          IDS_PASSWORD_MANAGER_PASSKEY_FROM_PROVIDER,
          base::UTF8ToUTF16(*credential.provider_name)));
    } else if (credential.source == AuthenticatorType::kPhone) {
      passkey.SetAuthenticatorLabel(l10n_util::GetStringFUTF16(
          IDS_PASSWORD_MANAGER_PASSKEY_FROM_PHONE, *priority_phone_name));
    }
  }
  ReportConditionalUiPasskeyCount(credentials.size());

  // TODO(https://crbug.com/358119268): This will probably get its own mediation
  // type, but for prototyping we assume any conditional request with passwords
  // uses ambient.
  bool has_ambient_credentials = !credentials.empty() || !passwords_.empty();
  if (base::FeatureList::IsEnabled(device::kWebAuthnAmbientSignin) &&
      has_ambient_credentials &&
      (credential_types_ &
       static_cast<int>(blink::mojom::CredentialTypeFlags::kPassword))) {
    auto* controller =
        ambient_signin::AmbientSigninController::GetOrCreateForCurrentDocument(
            render_frame_host);
    // TODO(https://crbug.com/358119268): `AmbientSigninController` needs to be
    // refactored, since this is now the single source of all credentials it
    // shows.
    controller->AddAndShowWebAuthnMethods(
        model(), credentials, credential_types_,
        base::BindOnce(
            IgnoreResult(
                &AuthenticatorRequestDialogController::OnAccountPreselected),
            weak_factory_.GetWeakPtr()));
    controller->AddAndShowPasswordMethods(
        std::move(passwords_), credential_types_,
        base::BindRepeating(
            &AuthenticatorRequestDialogModel::OnPasswordCredentialSelected,
            base::Unretained(model_)));
  }

  ChromeWebAuthnCredentialsDelegate* webauthn_credentials_delegate =
      ChromeWebAuthnCredentialsDelegateFactory::GetFactory(web_contents)
          ->GetDelegateForFrame(render_frame_host);
  if (webauthn_credentials_delegate) {
    // May be null on tests.
    webauthn_credentials_delegate->OnCredentialsReceived(
        std::move(credentials),
        ChromeWebAuthnCredentialsDelegate::SecurityKeyOrHybridFlowAvailable(
            true));
  }
  SetCurrentStep(Step::kPasskeyAutofill);
}

void AuthenticatorRequestDialogController::DispatchRequestAsync(
    AuthenticatorReference* authenticator) {
  // Dispatching to the same authenticator twice may result in unexpected
  // behavior.
  if (authenticator->dispatched || !request_callback_) {
    return;
  }

  authenticator->dispatched = true;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(request_callback_, authenticator->authenticator_id));
}

void AuthenticatorRequestDialogController::SortRecognizedCredentials() {
  struct {
    bool operator()(const device::DiscoverableCredentialMetadata& a,
                    const device::DiscoverableCredentialMetadata& b) {
      return std::tie(a.user.id, a.cred_id) < std::tie(b.user.id, b.cred_id);
    }
  } id_comparator;
  std::ranges::sort(transport_availability_.recognized_credentials,
                    std::ref(id_comparator));

  struct UsernameComparator {
    explicit UsernameComparator(const icu::Locale* locale) {
      UErrorCode error = U_ZERO_ERROR;
      collator_.reset(icu::Collator::createInstance(*locale, error));
    }

    bool operator()(const device::DiscoverableCredentialMetadata& a,
                    const device::DiscoverableCredentialMetadata& b) {
      return base::i18n::CompareString16WithCollator(
                 *collator_, base::UTF8ToUTF16(a.user.name.value_or("")),
                 base::UTF8ToUTF16(b.user.name.value_or(""))) == UCOL_LESS;
    }

    std::unique_ptr<icu::Collator> collator_;
  };
  UsernameComparator user_name_comparator(&icu::Locale::getDefault());

  std::ranges::stable_sort(transport_availability_.recognized_credentials,
                           std::ref(user_name_comparator));
}

void AuthenticatorRequestDialogController::PopulateMechanisms() {
  const bool is_get_assertion =
      transport_availability_.request_type == FidoRequestType::kGetAssertion;
  bool specific_local_passkeys_listed = false;
  if (is_get_assertion && IsModalRequest(ui_presentation())) {
    // List passkeys instead of mechanisms for platform & GPM authenticators.
    for (const auto& cred : transport_availability_.recognized_credentials) {
      if (cred.source == AuthenticatorType::kICloudKeychain &&
          !allow_icloud_keychain_) {
        continue;
      }
      if (cred.source == AuthenticatorType::kEnclave) {
        if (enclave_enabled_status_ != EnclaveEnabledStatus::kEnabled) {
          // Do not list passkeys from the enclave if it needs reauth before
          // proceeding.  Instead, we'll show a button to trigger reauth.
          continue;
        }
      }
      specific_local_passkeys_listed = true;
      std::u16string name = base::UTF8ToUTF16(cred.user.name.value_or(""));
      Mechanism::Type mechanism_type = Mechanism::Credential(
          {cred.source, cred.user.id, cred.last_used_time});
      auto& mechanism = model_->mechanisms.emplace_back(
          mechanism_type, name,
          GetMechanismIcon(mechanism_type, ui_presentation()),
          base::BindRepeating(
              base::IgnoreResult(
                  &AuthenticatorRequestDialogController::OnAccountPreselected),
              base::Unretained(this), cred.cred_id),
          base::UTF8ToUTF16(cred.user.display_name.value_or("")));
      mechanism.description =
          AuthenticatorRequestDialogModel::GetMechanismDescription(
              cred, ui_presentation());
    }
    if (!passwords_.empty()) {
      PopulatePasswords();
    }
  }

  std::vector<AuthenticatorTransport> transports_to_list_if_active;
  // Do not list the internal transport if we can offer users to select a
  // platform credential directly. This is true for both conditional requests
  // and the new passkey selector UI.
  bool did_enumerate_local_passkeys = false;
  if (ui_presentation() == UIPresentation::kAutofill) {
    did_enumerate_local_passkeys = true;
  } else if (is_get_assertion) {
    switch (transport_availability_.has_platform_authenticator_credential) {
      case device::FidoRequestHandlerBase::RecognizedCredential::
          kNoRecognizedCredential:
        did_enumerate_local_passkeys = true;
        break;
      case device::FidoRequestHandlerBase::RecognizedCredential::
          kHasRecognizedCredential:
        // Some platform authenticators (like ChromeOS) will report passkey
        // availability but will not enumerate them.
        did_enumerate_local_passkeys = specific_local_passkeys_listed;
        break;
      case device::FidoRequestHandlerBase::RecognizedCredential::kUnknown:
        did_enumerate_local_passkeys = false;
        break;
    }
  }
  if (!did_enumerate_local_passkeys &&
      base::Contains(transport_availability_.available_transports,
                     AuthenticatorTransport::kInternal)) {
    transports_to_list_if_active.push_back(AuthenticatorTransport::kInternal);
  }

  const auto kCable = AuthenticatorTransport::kHybrid;
  const bool windows_handles_hybrid = WebAuthnApiSupportsHybrid();
  bool include_add_phone_option = false;

  if (model_->cable_ui_type) {
    switch (*model_->cable_ui_type) {
      case AuthenticatorRequestDialogModel::CableUIType::CABLE_V2_2ND_FACTOR:
        if (base::Contains(transport_availability_.available_transports,
                           kCable)) {
          include_add_phone_option = !windows_handles_hybrid;
        }
        break;

      case AuthenticatorRequestDialogModel::CableUIType::CABLE_V2_SERVER_LINK:
      case AuthenticatorRequestDialogModel::CableUIType::CABLE_V1: {
        if (base::Contains(transport_availability_.available_transports,
                           kCable)) {
          transports_to_list_if_active.push_back(kCable);

          // If this is a caBLEv1 or server-link request then offering to "Try
          // Again" is unfortunate because the server won't send another ping
          // to the phone. It is valid if trying to use USB devices but the
          // confusion of the caBLE case overrides that.
          model_->offer_try_again_in_ui = false;
        }
        break;
      }
    }
  }

  if (!is_get_assertion &&
      enclave_enabled_status_ == EnclaveEnabledStatus::kEnabled &&
      *transport_availability_.make_credential_attachment !=
          device::AuthenticatorAttachment::kCrossPlatform) {
    const std::u16string name =
        l10n_util::GetStringUTF16(IDS_WEBAUTHN_SOURCE_GOOGLE_PASSWORD_MANAGER);
    Mechanism::Type mechanism_type = Mechanism::Enclave();
    Mechanism mechanism(
        mechanism_type, name,
        GetMechanismIcon(mechanism_type, ui_presentation()),
        base::BindRepeating(&AuthenticatorRequestDialogController::StartEnclave,
                            base::Unretained(this)));
    mechanism.description = base::UTF8ToUTF16(model_->GetGpmAccountEmail());
    model_->mechanisms.emplace_back(std::move(mechanism));
  }
  if (enclave_enabled_status_ ==
          EnclaveEnabledStatus::kEnabledAndReauthNeeded &&
      UIPresentation::kModal == ui_presentation() &&
      model_->relying_party_id != "google.com") {
    // Show a button that lets the user sign in again to restore sync. This
    // cancels the request, so we can't do it for conditional UI requests.
    // TODO(crbug.com/345413738): add support for conditional UI.
    const std::u16string name =
        l10n_util::GetStringUTF16(IDS_WEBAUTHN_SIGN_IN_AGAIN_TITLE);
    Mechanism::Type mechanism_type = Mechanism::SignInAgain();
    Mechanism enclave(
        mechanism_type, name,
        GetMechanismIcon(mechanism_type, ui_presentation()),
        base::BindRepeating(
            &AuthenticatorRequestDialogController::ReauthForSyncRestore,
            base::Unretained(this)));
    enclave.description =
        l10n_util::GetStringUTF16(IDS_WEBAUTHN_SIGN_IN_AGAIN_DESCRIPTION);
    model_->mechanisms.emplace_back(std::move(enclave));
  }

  if (transport_availability_.has_icloud_keychain && allow_icloud_keychain_ &&
      // The mechanism for iCloud Keychain only appears for create(), or if
      // Chrome doesn't have permission to enumerate credentials and thus the
      // user needs a generic mechanism to trigger it.
      (!is_get_assertion ||
       (transport_availability_.has_icloud_keychain_credential ==
            device::FidoRequestHandlerBase::RecognizedCredential::kUnknown &&
        ui_presentation() != UIPresentation::kModalImmediate))) {
    const std::u16string name =
        l10n_util::GetStringUTF16(IDS_WEBAUTHN_TRANSPORT_ICLOUD_KEYCHAIN);
    Mechanism::Type mechanism_type = Mechanism::ICloudKeychain();
    model_->mechanisms.emplace_back(
        mechanism_type, name,
        GetMechanismIcon(mechanism_type, ui_presentation()),
        base::BindRepeating(
            &AuthenticatorRequestDialogController::StartICloudKeychain,
            base::Unretained(this)));
  }

  std::optional<std::pair<int, AuthenticatorTransport>> windows_button_label;
  windows_button_label =
      GetWindowsAPIButtonLabel(transport_availability_, ui_presentation());
  if (windows_button_label &&
      windows_button_label->second == AuthenticatorTransport::kInternal) {
    // Add the Windows button before phones if it can trigger Windows Hello.
    AddWindowsButton(windows_button_label->first, windows_button_label->second);
  }

  // If the new UI is enabled, only show USB as an option if the QR code is
  // not available, if tapping it would trigger a prompt to enable BLE, or if
  // hints suggest that hybrid and USB should be separate options.
  const bool include_usb_option =
      base::Contains(transport_availability_.available_transports,
                     AuthenticatorTransport::kUsbHumanInterfaceDevice) &&
      (!include_add_phone_option ||
       transport_availability_.ble_status != BleStatus::kOn ||
       hints_.transport == AuthenticatorTransport::kUsbHumanInterfaceDevice ||
       hints_.transport == AuthenticatorTransport::kHybrid);

  if (include_add_phone_option) {
    model_->show_security_key_on_qr_sheet =
        base::Contains(transport_availability_.available_transports,
                       AuthenticatorTransport::kUsbHumanInterfaceDevice) &&
        !include_usb_option;
    std::u16string label = l10n_util::GetStringUTF16(
        GetHybridButtonLabel(model_->show_security_key_on_qr_sheet));
    Mechanism::Type mechanism_type = Mechanism::Hybrid();
    model_->mechanisms.emplace_back(
        mechanism_type, label,
        GetMechanismIcon(mechanism_type, ui_presentation()),
        base::BindRepeating(
            &AuthenticatorRequestDialogController::StartHybridFlow,
            base::Unretained(this)));
  }
  if (include_usb_option) {
    transports_to_list_if_active.push_back(
        AuthenticatorTransport::kUsbHumanInterfaceDevice);
  }

  for (const auto transport : transports_to_list_if_active) {
    if (!base::Contains(transport_availability_.available_transports,
                        transport)) {
      continue;
    }

    Mechanism::Type mechanism_type = Mechanism::Transport(transport);
    model_->mechanisms.emplace_back(
        mechanism_type, GetTransportDescription(transport),
        GetMechanismIcon(mechanism_type, ui_presentation()),
        base::BindRepeating(
            &AuthenticatorRequestDialogController::StartGuidedFlowForTransport,
            base::Unretained(this), transport));
  }
  // Add the Windows native API button last if it does not do Windows Hello.
  if (windows_button_label &&
      windows_button_label->second != AuthenticatorTransport::kInternal) {
    AddWindowsButton(windows_button_label->first, windows_button_label->second);
  }

  model_->mechanisms = webauthn::sorting::SortMechanisms(
      std::move(model_->mechanisms), ui_presentation());
}

void AuthenticatorRequestDialogController::AddWindowsButton(
    int label,
    AuthenticatorTransport transport) {
  const std::u16string desc = l10n_util::GetStringUTF16(label);
  Mechanism::Type mechanism_type = Mechanism::WindowsAPI();
  model_->mechanisms.emplace_back(
      mechanism_type, desc,
      GetMechanismIcon(mechanism_type, ui_presentation(), transport),
      base::BindRepeating(
          &AuthenticatorRequestDialogController::StartWinNativeApi,
          base::Unretained(this)));
}

std::optional<size_t>
AuthenticatorRequestDialogController::IndexOfPriorityMechanism() {
  // Never pick a priority mechanism if we are showing the enclave reauth
  // button.
  if (enclave_enabled_status_ ==
          EnclaveEnabledStatus::kEnabledAndReauthNeeded &&
      IsModalRequest(ui_presentation())) {
    return std::nullopt;
  }

  switch (transport_availability_.request_type) {
    case FidoRequestType::kGetAssertion:
      return ui_presentation() == UIPresentation::kModalImmediate
                 ? IndexOfImmediateGetPriorityMechanism()
                 : IndexOfGetAssertionPriorityMechanism();
    case FidoRequestType::kMakeCredential:
      return IndexOfMakeCredentialPriorityMechanism();
  }
}

std::optional<size_t>
AuthenticatorRequestDialogController::IndexOfGetAssertionPriorityMechanism() {
  CHECK_EQ(transport_availability_.request_type,
           FidoRequestType::kGetAssertion);

  // If there is a single mechanism, go to that.
  if (model_->mechanisms.size() == 1) {
    return 0;
  }

  if (transport_availability_.has_empty_allow_list) {
    // The index and info of the credential that the UI should default to.
    std::optional<std::pair<size_t, const Mechanism::CredentialInfo*>>
        best_cred;
    bool multiple_distinct_creds = false;
    bool has_password = false;

    for (size_t i = 0; i < model_->mechanisms.size(); ++i) {
      const auto& type = model_->mechanisms[i].type;
      if (std::holds_alternative<Mechanism::Credential>(type)) {
        const Mechanism::CredentialInfo* cred_info =
            &std::get<Mechanism::Credential>(type).value();

        if (!best_cred.has_value()) {
          best_cred = std::make_pair(i, cred_info);
        } else if (best_cred->second->user_id == cred_info->user_id) {
          if (SourcePriority(cred_info->source) >
              SourcePriority(best_cred->second->source)) {
            best_cred = std::make_pair(i, cred_info);
          }
        } else {
          multiple_distinct_creds = true;
        }
      } else if (std::holds_alternative<Mechanism::Password>(type)) {
        has_password = true;
      }
    }
    // If one of the passkeys is a valid default, go to that.
    if (!has_password && !multiple_distinct_creds && best_cred.has_value() &&
        // Do not set Windows Hello credentials as priority mechanisms. Doing so
        // narrows the allow-list to that specific credential. But, since
        // Windows also handles other mechanisms, it's better to avoid narrowing
        // the allow list.
        // `StartGuidedFlowForMostLikelyTransportOrShowMechanismSelection()`
        // will jump to Windows if all the credentials are Windows Hello.
        best_cred->second->source != AuthenticatorType::kWinNative &&
        (best_cred->second->source != AuthenticatorType::kEnclave ||
         CanDefaultToEnclave(Profile::FromBrowserContext(
                                 GetRenderFrameHost()->GetBrowserContext())
                                 ->GetOriginalProfile()))) {
      return best_cred->first;
    }
  }

  // If it's caBLEv1, or server-linked caBLEv2, jump to that.
  if (model_->cable_ui_type) {
    switch (*model_->cable_ui_type) {
      case AuthenticatorRequestDialogModel::CableUIType::CABLE_V2_SERVER_LINK:
      case AuthenticatorRequestDialogModel::CableUIType::CABLE_V1:
        for (size_t i = 0; i < model_->mechanisms.size(); ++i) {
          if (model_->mechanisms[i].type ==
              Mechanism::Type(
                  Mechanism::Transport(AuthenticatorTransport::kHybrid))) {
            return i;
          }
        }
        break;
      case AuthenticatorRequestDialogModel::CableUIType::CABLE_V2_2ND_FACTOR:
        break;
    }
  }

  // For all other cases, go to the multi source passkey picker.
  return std::nullopt;
}

std::optional<size_t>
AuthenticatorRequestDialogController::IndexOfImmediateGetPriorityMechanism() {
  CHECK_EQ(transport_availability_.request_type,
           FidoRequestType::kGetAssertion);
  CHECK_EQ(ui_presentation(), UIPresentation::kModalImmediate);

  if (model_->mechanisms.size() != 1) {
    return std::nullopt;
  }

  const auto& mechanism = model_->mechanisms[0];

  const bool is_enclave =
      std::holds_alternative<Mechanism::Credential>(mechanism.type) &&
      (std::get<Mechanism::Credential>(mechanism.type).value().source ==
       AuthenticatorType::kEnclave);
  const bool chrome_does_uv_for_gpm =
      model_->gpm_uv_method.value_or(
          EnclaveUserVerificationMethod::kUnsatisfiable) ==
      EnclaveUserVerificationMethod::kUVKeyWithChromeUI;

  if (transport_availability_.autoselect_in_immediate_mediation) {
    bool is_password =
        std::holds_alternative<Mechanism::Password>(mechanism.type);
    bool is_chrome_profile =
        std::holds_alternative<Mechanism::Credential>(mechanism.type) &&
        std::get<Mechanism::Credential>(mechanism.type).value().source ==
            AuthenticatorType::kTouchID;
    if (is_password || is_chrome_profile ||
        (is_enclave && !chrome_does_uv_for_gpm)) {
      // Password and Chrome Profile UV does not display account details.
      // Similarly non-Chrome user verification UI for enclave passkeys does not
      // display the selected account details. Show the Chrome UI first.
      return std::nullopt;
    }
    return 0;
  }

  if (is_enclave && chrome_does_uv_for_gpm) {
    return 0;
  }
  return std::nullopt;
}

std::optional<size_t>
AuthenticatorRequestDialogController::IndexOfMakeCredentialPriorityMechanism() {
  CHECK_EQ(transport_availability_.request_type,
           FidoRequestType::kMakeCredential);

  if (model_->mechanisms.size() == 1) {
    return 0;
  } else if (model_->mechanisms.empty()) {
    return std::nullopt;
  }

  bool windows_handles_hybrid = WebAuthnApiSupportsHybrid();
  std::vector<Mechanism::Type> priority_list;

  // For non-cross-platform requests, we attempt to jump to the platform
  // authenticator and avoid showing the mechanism selection sheet.
  if (transport_availability_.make_credential_attachment !=
      device::AuthenticatorAttachment::kCrossPlatform) {
    Profile* profile =
        Profile::FromBrowserContext(GetRenderFrameHost()->GetBrowserContext())
            ->GetOriginalProfile();
    if (CanDefaultToEnclave(profile) &&
        enclave_enabled_status_ == EnclaveEnabledStatus::kEnabled) {
      priority_list.emplace_back(Mechanism::Enclave());
    }

    // If Windows Hello is enabled, jump to it if it's a candidate.
    if (transport_availability_.win_is_uvpaa) {
      priority_list.emplace_back(Mechanism::WindowsAPI());
    }

#if BUILDFLAG(IS_MAC)
    // For non-cross-platform try to trigger either platform authenticator.
    if (should_create_in_icloud_keychain_) {
      priority_list.emplace_back(Mechanism::ICloudKeychain());
    } else {
      priority_list.emplace_back(
          Mechanism::Transport(AuthenticatorTransport::kInternal));
    }
#endif
  }

  // If Windows handles hybrid, then jump to it because all remaining options
  // are handled by Windows.
  if (windows_handles_hybrid) {
    priority_list.emplace_back(Mechanism::WindowsAPI());
  }

  // If there are no platform authenticators, then show the QR code for
  // passkey requests unless the user might be able to select a paired phone.
  const bool is_passkey_request = model_->resident_key_requirement !=
                                  device::ResidentKeyRequirement::kDiscouraged;
  if (is_passkey_request) {
    priority_list.emplace_back(Mechanism::Hybrid());
  } else {
    priority_list.emplace_back(Mechanism::WindowsAPI());
  }

  if (hints_.transport) {
    // Hints were specified, make sure to consider USB and hybrid.
    priority_list.emplace_back(Mechanism::Transport(*hints_.transport));

    // Find the highest priority mechanism that matches the hint.
    for (const auto& priority_mechanism : priority_list) {
      if (!MechanismMatchesHint(priority_mechanism, *hints_.transport)) {
        continue;
      }
      std::optional<int> index = FindIndexOfFirstMechanismOfType(
          model_->mechanisms, priority_mechanism);
      if (index.has_value()) {
        return *index;
      }
    }
    // No mechanism matching `hints_` was found. Continue to return the highest
    // priority mechanism ignoring `hints_` instead.
  }

  for (const auto& priority_mechanism : priority_list) {
    std::optional<int> index =
        FindIndexOfFirstMechanismOfType(model_->mechanisms, priority_mechanism);
    if (index.has_value()) {
      return *index;
    }
  }

  return std::nullopt;
}

bool AuthenticatorRequestDialogController::CanDefaultToEnclave(
    Profile* profile) {
  const bool enclave_decline_limit_reached =
      profile->GetPrefs()->GetInteger(
          webauthn::pref_names::kEnclaveDeclinedGPMCredentialCreationCount) >=
      kMaxPriorityGPMCredentialCreations;
  // If a user has declined bootstrapping too many times then GPM will still
  // be available in the mechanism selection screen for credential creation,
  // but it can no longer be a priority mechanism.
  const bool enclave_bootstrap_limit_reached =
      profile->GetPrefs()->GetInteger(
          webauthn::pref_names::kEnclaveDeclinedGPMBootstrappingCount) >=
      device::enclave::kMaxGPMBootstrapPrompts;
  return !enclave_decline_limit_reached && !enclave_bootstrap_limit_reached;
}

content::RenderFrameHost*
AuthenticatorRequestDialogController::GetRenderFrameHost() const {
  return content::RenderFrameHost::FromID(frame_host_id_);
}

void AuthenticatorRequestDialogController::StartPasskeyUpgradeRequest() {
  SetCurrentStep(Step::kPasskeyUpgrade);

  if (!enclave_request_callback_) {
    RecordPasskeyUpgradeResultHistogram(PasskeyUpgradeResult::kGpmDisabled);
    FIDO_LOG(ERROR)
        << "Passkey upgrade request failed because GPM is disabled by policy.";
    PasskeyUpgradeFailed();
    return;
  }

  passkey_upgrade_request_controller_ =
      std::make_unique<PasskeyUpgradeRequestController>(
          GetRenderFrameHost(), std::move(enclave_request_callback_));
  passkey_upgrade_request_controller_->TryUpgradePasswordToPasskey(
      model_->relying_party_id, model_->user_entity.name.value_or(""),
      /*delegate=*/this);
}

void AuthenticatorRequestDialogController::PopulatePasswords() {
  for (const auto& password : passwords_) {
    Mechanism::Type mechanism_type = Mechanism::Password(
        AuthenticatorRequestDialogModel::Mechanism::PasswordInfo(
            password->date_last_used));
    Mechanism mechanism(
        mechanism_type, password->username_value,
        GetMechanismIcon(mechanism_type, ui_presentation()),
        base::BindRepeating(
            &AuthenticatorRequestDialogModel::OnPasswordCredentialSelected,
            base::Unretained(model_),
            std::make_pair(password->username_value,
                           password->password_value)));
    mechanism.description = l10n_util::GetStringUTF16(
        IDS_PASSWORD_MANAGER_PASSWORD_FROM_GOOGLE_PASSWORD_MANAGER);
    model_->mechanisms.emplace_back(std::move(mechanism));
  }
}
