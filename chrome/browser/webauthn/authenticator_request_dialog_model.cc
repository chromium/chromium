// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"

#include <algorithm>
#include <iterator>
#include <string_view>
#include <utility>

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/string_compare.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/webauthn/authenticator_request_bubble.h"
#include "chrome/browser/ui/webauthn/authenticator_request_dialog.h"
#include "chrome/browser/ui/webauthn/authenticator_request_window.h"
#include "chrome/browser/webauthn/authenticator_reference.h"
#include "chrome/browser/webauthn/authenticator_transport.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "chrome/browser/webauthn/webauthn_metrics_util.h"
#include "chrome/browser/webauthn/webauthn_pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/features.h"
#include "components/vector_icons/vector_icons.h"
#include "components/webauthn/core/browser/passkey_model_change.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/enclave/constants.h"
#include "device/fido/enclave/metrics.h"
#include "device/fido/features.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/pin.h"
#include "device/fido/platform_user_verification_policy.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/i18n/unicode/coll.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_elider.h"

#if BUILDFLAG(IS_WIN)
#include "device/fido/win/webauthn_api.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#include "crypto/scoped_lacontext.h"
#include "device/fido/mac/util.h"
#endif

namespace {

constexpr int kMaxPriorityGPMCredentialCreations = 2;

// StepUiType enumerates the different types of UI that can be displayed.
enum class StepUIType {
  NONE,
  // A Chromium captive dialog.
  DIALOG,
  // A Google Password Manager bubble.
  BUBBLE,
  // A top-level window.
  WINDOW,
};

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
    case AuthenticatorTransport::kAndroidAccessory:
      return IDS_WEBAUTHN_TRANSPORT_AOA;
    case AuthenticatorTransport::kBluetoothLowEnergy:
    case AuthenticatorTransport::kNearFieldCommunication:
      NOTREACHED();
      return 0;
  }
}

std::u16string GetTransportDescription(AuthenticatorTransport transport) {
  const int msg_id = GetMessageIdForTransportDescription(transport);
  if (!msg_id) {
    return std::u16string();
  }
  return l10n_util::GetStringUTF16(msg_id);
}

constexpr int GetMessageIdForTransportShortDescription(
    AuthenticatorTransport transport) {
  switch (transport) {
    case AuthenticatorTransport::kUsbHumanInterfaceDevice:
      return IDS_WEBAUTHN_TRANSPORT_POPUP_USB;
    case AuthenticatorTransport::kInternal:
      return IDS_WEBAUTHN_TRANSPORT_POPUP_INTERNAL;
    case AuthenticatorTransport::kHybrid:
      return IDS_WEBAUTHN_TRANSPORT_POPUP_CABLE;
    case AuthenticatorTransport::kAndroidAccessory:
      return IDS_WEBAUTHN_TRANSPORT_POPUP_AOA;
    case AuthenticatorTransport::kBluetoothLowEnergy:
    case AuthenticatorTransport::kNearFieldCommunication:
      NOTREACHED();
      return 0;
  }
}

std::u16string GetTransportShortDescription(AuthenticatorTransport transport) {
  const int msg_id = GetMessageIdForTransportShortDescription(transport);
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
    case AuthenticatorTransport::kAndroidAccessory:
      return kUsbCableIcon;
    case AuthenticatorTransport::kBluetoothLowEnergy:
    case AuthenticatorTransport::kNearFieldCommunication:
      NOTREACHED();
      return gfx::kNoneIcon;
  }
}

// Whether to show Step::kCreatePasskey, which prompts the user before platform
// authenticator dispatch during MakeCredential. This is currently only shown on
// MacOS, because that is the only desktop platform authenticator without a
// "native" WebAuthn UI.
constexpr bool kShowCreatePlatformPasskeyStep = BUILDFLAG(IS_MAC);

password_manager::PasskeyCredential::Source ToPasswordManagerSource(
    device::AuthenticatorType type) {
  switch (type) {
    case device::AuthenticatorType::kWinNative:
      return password_manager::PasskeyCredential::Source::kWindowsHello;
    case device::AuthenticatorType::kTouchID:
      return password_manager::PasskeyCredential::Source::kTouchId;
    case device::AuthenticatorType::kPhone:
      return password_manager::PasskeyCredential::Source::kAndroidPhone;
    case device::AuthenticatorType::kICloudKeychain:
      return password_manager::PasskeyCredential::Source::kICloudKeychain;
    case device::AuthenticatorType::kEnclave:
    case device::AuthenticatorType::kChromeOSPasskeys:
      return password_manager::PasskeyCredential::Source::
          kGooglePasswordManager;
    case device::AuthenticatorType::kChromeOS:
    case device::AuthenticatorType::kOther:
      return password_manager::PasskeyCredential::Source::kOther;
  }
}

// Stores the last used pairing in the user's profile if available.
void MaybeStoreLastUsedPairing(
    content::RenderFrameHost* rfh,
    const std::array<uint8_t, device::kP256X962Length>& pairing_public_key) {
  if (!rfh) {
    // The RFH might be null in unit tests, or it might not be alive anymore.
    return;
  }
  Profile* profile = Profile::FromBrowserContext(rfh->GetBrowserContext());
  profile->GetPrefs()->SetString(
      webauthn::pref_names::kLastUsedPairingFromSyncPublicKey,
      base::Base64Encode(pairing_public_key));
}

// Retrieves the last used pairing public key from the user's profile, if
// available.
std::optional<std::vector<uint8_t>> RetrieveLastUsedPairing(
    content::RenderFrameHost* rfh) {
  if (!rfh) {
    // The RFH might be null in unit tests, or it might not be alive anymore.
    return std::nullopt;
  }
  Profile* profile = Profile::FromBrowserContext(rfh->GetBrowserContext());
  std::string maybe_last_used_pairing = profile->GetPrefs()->GetString(
      webauthn::pref_names::kLastUsedPairingFromSyncPublicKey);
  std::optional<std::vector<uint8_t>> last_used_pairing;
  if (maybe_last_used_pairing.empty()) {
    return std::nullopt;
  }
  return base::Base64Decode(maybe_last_used_pairing);
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

const gfx::VectorIcon& GetCredentialIcon(device::AuthenticatorType type) {
  if (type == device::AuthenticatorType::kPhone) {
    return kSmartphoneIcon;
  }
  return vector_icons::kPasskeyIcon;
}

std::u16string GetMechanismDescription(
    device::AuthenticatorType type,
    const std::optional<std::string>& priority_phone_name) {
  if (type == device::AuthenticatorType::kPhone) {
    return l10n_util::GetStringFUTF16(IDS_WEBAUTHN_SOURCE_PHONE,
                                      base::UTF8ToUTF16(*priority_phone_name));
  }
  int message;
  bool enclave_enabled =
      base::FeatureList::IsEnabled(device::kWebAuthnEnclaveAuthenticator);
  switch (type) {
    case device::AuthenticatorType::kWinNative:
      message = enclave_enabled ? IDS_WEBAUTHN_SOURCE_WINDOWS_HELLO_NEW
                                : IDS_WEBAUTHN_SOURCE_WINDOWS_HELLO;
      break;
    case device::AuthenticatorType::kTouchID:
      message = enclave_enabled ? IDS_WEBAUTHN_SOURCE_CHROME_PROFILE_NEW
                                : IDS_WEBAUTHN_SOURCE_CHROME_PROFILE;
      break;
    case device::AuthenticatorType::kICloudKeychain:
      // TODO(crbug.com/40265798): Use IDS_WEBAUTHN_SOURCE_CUSTOM_VENDOR for
      // third party providers.
      message = enclave_enabled ? IDS_WEBAUTHN_SOURCE_ICLOUD_KEYCHAIN_NEW
                                : IDS_WEBAUTHN_SOURCE_ICLOUD_KEYCHAIN;
      break;
    case device::AuthenticatorType::kEnclave:
      CHECK(enclave_enabled);
      message = IDS_WEBAUTHN_SOURCE_GOOGLE_PASSWORD_MANAGER;
      break;
    default:
      message = IDS_PASSWORD_MANAGER_USE_GENERIC_DEVICE;
  }
  return l10n_util::GetStringUTF16(message);
}

int GetHybridButtonLabel(bool has_security_key, bool specific_phones_listed) {
  if (has_security_key) {
    return specific_phones_listed
               ? IDS_WEBAUTHN_PASSKEY_DIFFERENT_PHONE_TABLET_OR_SECURITY_KEY_LABEL
               : IDS_WEBAUTHN_PASSKEY_PHONE_TABLET_OR_SECURITY_KEY_LABEL;
  } else {
    return specific_phones_listed
               ? IDS_WEBAUTHN_PASSKEY_DIFFERENT_PHONE_OR_TABLET_LABEL
               : IDS_WEBAUTHN_PASSKEY_PHONE_OR_TABLET_LABEL;
  }
}

// SourcePriority determines which credential will be used when doing a modal
// get and multiple platform authenticators have credentials, all with the same
// user ID.
int SourcePriority(device::AuthenticatorType source) {
  switch (source) {
    case device::AuthenticatorType::kEnclave:
      return 4;
    case device::AuthenticatorType::kICloudKeychain:
      return 3;
    case device::AuthenticatorType::kTouchID:
      return 2;
    case device::AuthenticatorType::kWinNative:
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
    bool specific_phones_listed) {
  if (!transport_availability.has_win_native_api_authenticator) {
    return std::nullopt;
  }
  bool win_handles_internal;
  bool win_handles_hybrid;
  bool win_handles_security_key;
  if (transport_availability.request_type ==
      device::FidoRequestType::kGetAssertion) {
    win_handles_internal =
        (transport_availability.transport_list_did_include_internal ||
         transport_availability.has_empty_allow_list) &&
        transport_availability.has_platform_authenticator_credential ==
            device::FidoRequestHandlerBase::RecognizedCredential::kUnknown;
    win_handles_hybrid =
        (transport_availability.transport_list_did_include_hybrid ||
         transport_availability.has_empty_allow_list) &&
        WebAuthnApiSupportsHybrid();
    win_handles_security_key =
        transport_availability.transport_list_did_include_security_key ||
        transport_availability.has_empty_allow_list;
  } else {
    win_handles_internal = transport_availability.make_credential_attachment ==
                               device::AuthenticatorAttachment::kPlatform ||
                           transport_availability.make_credential_attachment ==
                               device::AuthenticatorAttachment::kAny;
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
    return std::make_pair(
        GetHybridButtonLabel(win_handles_security_key, specific_phones_listed),
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
bool IsChromeImplemented(device::AuthenticatorType type) {
  // Note: it must never be possible for any machine to observe two different
  // sources of "Chrome implemented" credentials. I.e. a given platform only
  // ever has one type of Chrome-implemented platform authenticator.
  // This is CHECKed in `StartFlow`.
  switch (type) {
    case device::AuthenticatorType::kWinNative:
      return false;
    case device::AuthenticatorType::kTouchID:
      return true;
    case device::AuthenticatorType::kChromeOS:
      return true;
    case device::AuthenticatorType::kPhone:
      return false;
    case device::AuthenticatorType::kICloudKeychain:
      return false;
    case device::AuthenticatorType::kEnclave:
      return false;
    case device::AuthenticatorType::kChromeOSPasskeys:
      return false;
    case device::AuthenticatorType::kOther:
      // For testing purposes.
      return true;
  }
}

StepUIType step_ui_type(AuthenticatorRequestDialogModel::Step step) {
  switch (step) {
    case AuthenticatorRequestDialogModel::Step::kClosed:
    case AuthenticatorRequestDialogModel::Step::kNotStarted:
    case AuthenticatorRequestDialogModel::Step::kConditionalMediation:
      return StepUIType::NONE;

    case AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain:
    case AuthenticatorRequestDialogModel::Step::kGPMReauthForPinReset:
      return StepUIType::WINDOW;

    case AuthenticatorRequestDialogModel::Step::kGPMPasskeySaved:
      return StepUIType::BUBBLE;

    default:
      return StepUIType::DIALOG;
  }
}

std::optional<content::GlobalRenderFrameHostId> FrameHostIdFromMaybeNull(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host == nullptr) {
    return std::nullopt;
  }
  return render_frame_host->GetGlobalId();
}

bool HaveTouchId() {
#if BUILDFLAG(IS_MAC)
  return device::fido::mac::DeviceHasBiometricsAvailable();
#else
  return false;
#endif
}

}  // namespace

#define AUTHENTICATOR_REQUEST_EVENT_0(name) \
  void AuthenticatorRequestDialogModel::Observer::name() {}
#define AUTHENTICATOR_REQUEST_EVENT_1(name, arg1type) \
  void AuthenticatorRequestDialogModel::Observer::name(arg1type) {}
AUTHENTICATOR_EVENTS
#undef AUTHENTICATOR_REQUEST_EVENT_0
#undef AUTHENTICATOR_REQUEST_EVENT_1

AuthenticatorRequestDialogModel::AuthenticatorRequestDialogModel(
    content::RenderFrameHost* render_frame_host)
    : frame_host_id(FrameHostIdFromMaybeNull(render_frame_host)) {}

AuthenticatorRequestDialogModel::~AuthenticatorRequestDialogModel() {
  for (auto& observer : observers) {
    observer.OnModelDestroyed(this);
  }
}

void AuthenticatorRequestDialogModel::AddObserver(
    AuthenticatorRequestDialogModel::Observer* observer) {
  observers.AddObserver(observer);
}

void AuthenticatorRequestDialogModel::RemoveObserver(
    AuthenticatorRequestDialogModel::Observer* observer) {
  observers.RemoveObserver(observer);
}

void AuthenticatorRequestDialogModel::SetStep(Step step) {
  const StepUIType previous_ui_type = step_ui_type(step_);
  step_ = step;
  ui_disabled_ = false;

  const StepUIType ui_type = step_ui_type(step_);
  auto* web_contents = GetWebContents();
  if (previous_ui_type != ui_type && web_contents) {
    // The UI observes `OnStepTransition` and updates automatically.
    switch (ui_type) {
      case StepUIType::NONE:
        // Any UI will close itself.
        break;

      case StepUIType::DIALOG:
        ShowAuthenticatorRequestDialog(web_contents, this);
        break;

      case StepUIType::BUBBLE:
        ShowAuthenticatorRequestBubble(web_contents, this);
        break;

      case StepUIType::WINDOW:
        ShowAuthenticatorRequestWindow(web_contents, this);
        break;
    }
  }

  for (auto& observer : observers) {
    observer.OnStepTransition();
  }
}

content::WebContents* AuthenticatorRequestDialogModel::GetWebContents() const {
  if (!frame_host_id) {
    return nullptr;
  }
  return content::WebContents::FromRenderFrameHost(
      content::RenderFrameHost::FromID(*frame_host_id));
}

content::RenderFrameHost* AuthenticatorRequestDialogModel::GetRenderFrameHost()
    const {
  if (!frame_host_id) {
    return nullptr;
  }
  return content::RenderFrameHost::FromID(*frame_host_id);
}

bool AuthenticatorRequestDialogModel::should_dialog_be_closed() const {
  return step_ui_type(step_) != StepUIType::DIALOG;
}

bool AuthenticatorRequestDialogModel::should_bubble_be_closed() const {
  return step_ui_type(step_) != StepUIType::BUBBLE;
}

#define AUTHENTICATOR_REQUEST_EVENT_0(name)      \
  void AuthenticatorRequestDialogModel::name() { \
    for (auto& observer : observers) {           \
      observer.name();                           \
    }                                            \
  }
#define AUTHENTICATOR_REQUEST_EVENT_1(name, arg1type)         \
  void AuthenticatorRequestDialogModel::name(arg1type arg1) { \
    for (auto& observer : observers) {                        \
      observer.name(arg1);                                    \
    }                                                         \
  }
AUTHENTICATOR_EVENTS
#undef AUTHENTICATOR_REQUEST_EVENT_0
#undef AUTHENTICATOR_REQUEST_EVENT_1

AuthenticatorRequestDialogController::EphemeralState::EphemeralState() =
    default;
AuthenticatorRequestDialogController::EphemeralState::EphemeralState(
    EphemeralState&&) = default;
AuthenticatorRequestDialogController::EphemeralState&
AuthenticatorRequestDialogController::EphemeralState::operator=(
    EphemeralState&&) = default;
AuthenticatorRequestDialogController::EphemeralState::~EphemeralState() =
    default;

AuthenticatorRequestDialogModel::Mechanism::Mechanism(
    AuthenticatorRequestDialogModel::Mechanism::Type in_type,
    std::u16string in_name,
    std::u16string in_short_name,
    const gfx::VectorIcon& in_icon,
    base::RepeatingClosure in_callback)
    : type(std::move(in_type)),
      name(std::move(in_name)),
      short_name(std::move(in_short_name)),
      icon(in_icon),
      callback(std::move(in_callback)) {}
AuthenticatorRequestDialogModel::Mechanism::~Mechanism() = default;
AuthenticatorRequestDialogModel::Mechanism::Mechanism(Mechanism&&) = default;

AuthenticatorRequestDialogModel::Mechanism::CredentialInfo::CredentialInfo(
    device::AuthenticatorType source_in,
    std::vector<uint8_t> user_id_in)
    : source(source_in), user_id(std::move(user_id_in)) {}
AuthenticatorRequestDialogModel::Mechanism::CredentialInfo::CredentialInfo(
    const CredentialInfo&) = default;
AuthenticatorRequestDialogModel::Mechanism::CredentialInfo::~CredentialInfo() =
    default;
bool AuthenticatorRequestDialogModel::Mechanism::CredentialInfo::operator==(
    const CredentialInfo&) const = default;

void AuthenticatorRequestDialogController::ResetEphemeralState() {
  ephemeral_state_ = {};
  model_->selected_phone_name.reset();
  model_->creds.clear();
  model_->priority_mechanism_index.reset();
}

AuthenticatorRequestDialogController::AuthenticatorRequestDialogController(
    AuthenticatorRequestDialogModel* model)
    : model_(model) {
  model_->observers.AddObserver(this);
  content::RenderFrameHost* frame_host = model_->GetRenderFrameHost();
  if (frame_host &&
      base::FeatureList::IsEnabled(syncer::kSyncWebauthnCredentials)) {
    webauthn::PasskeyModel* passkey_model =
        PasskeyModelFactory::GetInstance()->GetForProfile(
            Profile::FromBrowserContext(frame_host->GetBrowserContext()));
    if (passkey_model) {
      passkey_model_observation_.Observe(passkey_model);
    }
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

void AuthenticatorRequestDialogController::HideDialog() {
  SetCurrentStep(Step::kNotStarted);
}

void AuthenticatorRequestDialogController::StartFlow(
    TransportAvailabilityInfo transport_availability,
    bool use_conditional_mediation) {
  DCHECK(!started_);
  DCHECK_EQ(model_->step(), Step::kNotStarted);

  started_ = true;
  transport_availability_ = std::move(transport_availability);
  UpdateModelForTransportAvailability();
  use_conditional_mediation_ = use_conditional_mediation;

  if (base::FeatureList::IsEnabled(
          device::kWebAuthnChromeImplementedInvariant)) {
    // All recognised credentials that are "Chrome implemented" are from the
    // same source, i.e. a platform never has two Chrome implemented platform
    // authenticators.
    std::optional<device::AuthenticatorType> chrome_implemented_type;
    for (const auto& cred : transport_availability_.recognized_credentials) {
      if (IsChromeImplemented(cred.source)) {
        if (chrome_implemented_type.has_value()) {
          CHECK_EQ(*chrome_implemented_type, cred.source);
        } else {
          chrome_implemented_type = cred.source;
        }
      }
    }
  }

  SortRecognizedCredentials();

#if BUILDFLAG(IS_MAC)
  RecordMacOsStartedHistogram();
#endif

  PopulateMechanisms();
  model_->priority_mechanism_index = IndexOfPriorityMechanism();

  if (use_conditional_mediation_) {
    // This is a conditional mediation request.
    StartConditionalMediationRequest();
  } else {
    StartGuidedFlowForMostLikelyTransportOrShowMechanismSelection();
  }
}

void AuthenticatorRequestDialogController::StartOver() {
  if (model_->step() == Step::kTrustThisComputerCreation ||
      model_->step() == Step::kTrustThisComputerAssertion) {
    auto* pref_service = Profile::FromBrowserContext(
                             model_->GetRenderFrameHost()->GetBrowserContext())
                             ->GetOriginalProfile()
                             ->GetPrefs();
    int current_gpm_decline_count = pref_service->GetInteger(
        webauthn::pref_names::kEnclaveDeclinedGPMBootstrappingCount);
    pref_service->SetInteger(
        webauthn::pref_names::kEnclaveDeclinedGPMBootstrappingCount,
        std::min(current_gpm_decline_count + 1,
                 device::enclave::kMaxGPMBootstrapPrompts));
  } else if (enclave_was_priority_mechanism_) {
    auto* pref_service = Profile::FromBrowserContext(
                             model_->GetRenderFrameHost()->GetBrowserContext())
                             ->GetOriginalProfile()
                             ->GetPrefs();
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

void AuthenticatorRequestDialogController::TransitionToModalWebAuthnRequest() {
  DCHECK_EQ(model_->step(), Step::kConditionalMediation);

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
  const bool will_do_uv = device::fido::PlatformWillDoUserVerification(
      transport_availability_.user_verification_requirement);

  if (pending_step_) {
    SetCurrentStep(*pending_step_);
    pending_step_.reset();
  } else if (model_->mechanisms.empty()) {
    if (transport_availability_.transport_list_did_include_internal) {
      SetCurrentStep(Step::kErrorNoPasskeys);
    } else {
      SetCurrentStep(Step::kErrorNoAvailableTransports);
    }
  } else if (transport_availability_.request_type ==
                 device::FidoRequestType::kMakeCredential &&
             hints_.transport &&
             StartGuidedFlowForMakeCredentialFromHint(*hints_.transport)) {
  } else if (model_->priority_mechanism_index) {
    Mechanism& mechanism =
        model_->mechanisms[*model_->priority_mechanism_index];
    const Mechanism::Credential* cred =
        absl::get_if<Mechanism::Credential>(&mechanism.type);

    // If the authenticator will show its own confirmation then we don't want to
    // duplicate it.
    const bool authenticator_shows_own_confirmation =
        cred &&
        (cred->value().source == device::AuthenticatorType::kICloudKeychain ||
         // The enclave Touch ID prompts shows the credential details.
         (cred->value().source == device::AuthenticatorType::kEnclave &&
          will_do_uv && HaveTouchId()));

    if (cred != nullptr &&
        // Credentials on phones should never be triggered automatically.
        (cred->value().source == device::AuthenticatorType::kPhone ||
         // In the case of an empty allow list, the user should be able to see
         // the account that they're signing in with. So either
         // `kSelectPriorityMechanism` is used or else the authenticator shows
         // their own UI.
         (transport_availability_.has_empty_allow_list &&
          !authenticator_shows_own_confirmation) ||
         // Never auto-trigger macOS profile credentials without either a local
         // biometric or a UV requirement because, otherwise, there'll not be
         // *any* UI.
         (cred->value().source == device::AuthenticatorType::kTouchID &&
          !will_do_uv))) {
      SetCurrentStep(Step::kSelectPriorityMechanism);
    } else if (cred != nullptr || !hints_.transport.has_value() ||
               transport_availability_.request_type !=
                   device::FidoRequestType::kGetAssertion ||
               !StartGuidedFlowForGetAssertionFromHint(*hints_.transport)) {
      if (absl::holds_alternative<Mechanism::Enclave>(mechanism.type)) {
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
    // authenticators so `priority_mechanism_index_` cannot handle this.
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
      // If not doing UV, but the allowlist matches an enclave credential,
      // show UI to serve as user presence.
      if (!will_do_uv && transport_availability_.request_type ==
                             device::FidoRequestType::kGetAssertion) {
        for (auto& cred : transport_availability_.recognized_credentials) {
          if (cred.source == device::AuthenticatorType::kEnclave) {
            model_->creds = {cred};
            SetCurrentStep(Step::kPreSelectSingleAccount);
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
          if (absl::holds_alternative<Mechanism::Credential>(type)) {
            if (absl::get<Mechanism::Credential>(type)->source ==
                device::AuthenticatorType::kEnclave) {
              CHECK(will_do_uv);
              mechanism.callback.Run();
              return;
            }
            if (absl::get<Mechanism::Credential>(type)->source ==
                device::AuthenticatorType::kPhone) {
              SetCurrentStep(Step::kPhoneConfirmationSheet);
              return;
            }
          }
        }
      }
    }
    // If a request only includes mechanisms that can be serviced by the Windows
    // API and local credentials, there is no point showing Chrome UI as an
    // extra step. Jump to Windows instead.
    if (transport_availability_.has_win_native_api_authenticator &&
        base::ranges::all_of(model_->mechanisms, [](const auto& mech) {
          return absl::holds_alternative<Mechanism::WindowsAPI>(mech.type) ||
                 (absl::holds_alternative<Mechanism::Credential>(mech.type) &&
                  absl::get<Mechanism::Credential>(mech.type).value().source ==
                      device::AuthenticatorType::kWinNative);
        })) {
      ephemeral_state_.did_invoke_platform_despite_no_priority_mechanism_ =
          true;
      StartWinNativeApi();
      return;
    }
    if (!hints_.transport.has_value() ||
        transport_availability_.request_type !=
            device::FidoRequestType::kGetAssertion ||
        // If there were any matches, ignore a hint and show the user the list.
        base::ranges::any_of(model_->mechanisms,
                             [](const auto& mech) {
                               return absl::get_if<Mechanism::Credential>(
                                   &mech.type);
                             }) ||
        !StartGuidedFlowForGetAssertionFromHint(*hints_.transport)) {
      SetCurrentStep(Step::kMechanismSelection);
    }
  }
}

bool AuthenticatorRequestDialogController::
    StartGuidedFlowForMakeCredentialFromHint(AuthenticatorTransport transport) {
  CHECK_EQ(transport_availability_.request_type,
           device::FidoRequestType::kMakeCredential);

  // The RP has given a hint about the expected transport for a create() call.
  // See https://w3c.github.io/webauthn/#enum-hints
  switch (*hints_.transport) {
    case AuthenticatorTransport::kUsbHumanInterfaceDevice:
      if (transport_availability_.has_win_native_api_authenticator) {
        StartWinNativeApi();
      } else if (base::Contains(
                     transport_availability_.available_transports,
                     AuthenticatorTransport::kUsbHumanInterfaceDevice)) {
        StartGuidedFlowForTransport(*hints_.transport);
      } else {
        return false;
      }
      break;
    case AuthenticatorTransport::kHybrid:
      if (WebAuthnApiSupportsHybrid()) {
        StartWinNativeApi();
      } else if (base::Contains(transport_availability_.available_transports,
                                AuthenticatorTransport::kHybrid)) {
        if (!paired_phones_.empty()) {
          SetCurrentStep(Step::kMechanismSelection);
        } else {
          StartGuidedFlowForAddPhone();
        }
      } else {
        return false;
      }
      break;
    case AuthenticatorTransport::kInternal:
      if (transport_availability_.has_win_native_api_authenticator) {
        StartWinNativeApi();
      } else if (transport_availability_.has_icloud_keychain &&
                 should_create_in_icloud_keychain_) {
        StartICloudKeychain();
      } else if (base::Contains(transport_availability_.available_transports,
                                AuthenticatorTransport::kInternal)) {
        StartGuidedFlowForTransport(*hints_.transport);
      } else {
        return false;
      }
      break;
    default:
      NOTREACHED();
      return false;
  }
  return true;
}

bool AuthenticatorRequestDialogController::
    StartGuidedFlowForGetAssertionFromHint(AuthenticatorTransport transport) {
  CHECK_EQ(transport_availability_.request_type,
           device::FidoRequestType::kGetAssertion);

  // The RP has given a hint about the expected transport for a get() call.
  // See https://w3c.github.io/webauthn/#enum-hints
  switch (*hints_.transport) {
    case AuthenticatorTransport::kUsbHumanInterfaceDevice:
      if (transport_availability_.has_win_native_api_authenticator) {
        StartWinNativeApi();
      } else if (base::Contains(
                     transport_availability_.available_transports,
                     AuthenticatorTransport::kUsbHumanInterfaceDevice)) {
        StartGuidedFlowForTransport(*hints_.transport);
      } else {
        return false;
      }
      break;
    case AuthenticatorTransport::kHybrid:
      if (WebAuthnApiSupportsHybrid()) {
        StartWinNativeApi();
      } else if (base::Contains(transport_availability_.available_transports,
                                AuthenticatorTransport::kHybrid)) {
        if (base::ranges::any_of(model_->mechanisms, [](const auto& mechanism) {
              return absl::get_if<Mechanism::Phone>(&mechanism.type) != nullptr;
            })) {
          SetCurrentStep(Step::kMechanismSelection);
        } else {
          StartGuidedFlowForAddPhone();
        }
      } else {
        return false;
      }
      return true;
    case AuthenticatorTransport::kInternal:
      // If we can enumerate platform credentials, and there's a match, we'll
      // either jump to it immediately, or show an account selector.
      if (transport_availability_.has_win_native_api_authenticator) {
        StartWinNativeApi();
      } else {
        // We might not be able to enumerate iCloud Keychain because of
        // permissions issues, but that UI is bit limiting once we have jumped
        // to it, and people who have denied Chrome that permission are a bit of
        // a corner case, so we'll not currently jump to iCloud Keychain based
        // on this hint. (It's only a click away.)
        return false;
      }
      break;
    default:
      NOTREACHED();
      return false;
  }

  return true;
}

void AuthenticatorRequestDialogController::OnPhoneContactFailed(
    const std::string& name) {
  ContactNextPhoneByName(name);
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

void AuthenticatorRequestDialogController::OnRecoverSecurityDomainClosed() {
  // TODO(enclave): implement this.
}

void AuthenticatorRequestDialogController::StartPhonePairing() {
  DCHECK(model_->cable_qr_string);
  SetCurrentStep(Step::kCableV2QRCode);
}

void AuthenticatorRequestDialogController::
    EnsureBleAdapterIsPoweredAndContinueWithStep(Step step) {
  DCHECK(model_->step() == Step::kMechanismSelection ||
         model_->step() == Step::kUsbInsertAndActivate ||
         model_->step() == Step::kCableActivate ||
         model_->step() == Step::kAndroidAccessory ||
         model_->step() == Step::kOffTheRecordInterstitial ||
         model_->step() == Step::kPreSelectAccount ||
         model_->step() == Step::kSelectPriorityMechanism ||
         model_->step() == Step::kSelectAccount ||
         model_->step() == Step::kConditionalMediation ||
         model_->step() == Step::kNotStarted)
      << "Invalid step " << static_cast<int>(model_->step());

#if BUILDFLAG(IS_MAC)
  if (transport_availability()->ble_access_denied) {
    // |step| is not saved because macOS asks the user to restart Chrome
    // after permission has been granted. So the user will end up retrying
    // the whole WebAuthn request in the new process.
    SetCurrentStep(Step::kBlePermissionMac);
    return;
  }
#endif

  if (model_->ble_adapter_is_powered) {
    SetCurrentStep(step);
    return;
  }

  after_ble_adapter_powered_ =
      base::BindOnce(&AuthenticatorRequestDialogController::SetCurrentStep,
                     weak_factory_.GetWeakPtr(), step);

  if (transport_availability()->can_power_on_ble_adapter) {
    SetCurrentStep(Step::kBlePowerOnAutomatic);
  } else {
    SetCurrentStep(Step::kBlePowerOnManual);
  }
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

void AuthenticatorRequestDialogController::TryUsbDevice() {
  DCHECK_EQ(model_->step(), Step::kUsbInsertAndActivate);
}

void AuthenticatorRequestDialogController::StartPlatformAuthenticatorFlow() {
  if (transport_availability_.request_type ==
      device::FidoRequestType::kGetAssertion) {
    switch (transport_availability_.has_platform_authenticator_credential) {
      case device::FidoRequestHandlerBase::RecognizedCredential::kUnknown:
        CHECK(false);
        break;
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
        SetCurrentStep(model_->creds.size() == 1 ? Step::kPreSelectSingleAccount
                                                 : Step::kPreSelectAccount);
      } else {
        // For requests with an allow list, pre-select a random credential.
        model_->creds = {platform_credentials.front()};
#if BUILDFLAG(IS_MAC)
        if (device::fido::PlatformWillDoUserVerification(
                transport_availability_.user_verification_requirement)) {
          // If it's not preferable to complete the request by clicking
          // "Continue" then don't show the account selection sheet.
          HideDialogAndDispatchToPlatformAuthenticator();
        } else  // NOLINT(readability/braces)
#endif
        {
          // Otherwise show the chosen credential to the user. For platform
          // authenticators with optional UV (e.g. Touch ID), this step
          // essentially acts as the user presence check.
          SetCurrentStep(Step::kPreSelectSingleAccount);
        }
      }
      return;
    }
  }

  if (transport_availability_.request_type ==
      device::FidoRequestType::kMakeCredential) {
    if (kShowCreatePlatformPasskeyStep) {
      SetCurrentStep(Step::kCreatePasskey);
      return;
    }

    if (transport_availability_.is_off_the_record_context) {
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

void AuthenticatorRequestDialogController::
    OnOffTheRecordInterstitialAccepted() {
  std::move(after_off_the_record_interstitial_).Run();
}

void AuthenticatorRequestDialogController::ShowCableUsbFallback() {
  DCHECK_EQ(model_->step(), Step::kCableActivate);
  SetCurrentStep(Step::kAndroidAccessory);
}

void AuthenticatorRequestDialogController::ShowCable() {
  DCHECK_EQ(model_->step(), Step::kAndroidAccessory);
  SetCurrentStep(Step::kCableActivate);
}

void AuthenticatorRequestDialogController::CancelAuthenticatorRequest() {
  if (use_conditional_mediation_) {
    // Conditional UI requests are never cancelled, they restart silently.
    ResetEphemeralState();
    for (auto& observer : model_->observers) {
      observer.OnStartOver();
    }
    StartConditionalMediationRequest();
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
  if (use_conditional_mediation_) {
    auto* render_frame_host = model_->GetRenderFrameHost();
    auto* web_contents = model_->GetWebContents();
    if (web_contents && render_frame_host) {
      ChromeWebAuthnCredentialsDelegateFactory::GetFactory(web_contents)
          ->GetDelegateForFrame(render_frame_host)
          ->NotifyWebAuthnRequestAborted();
    }
  }
  if (model_->step() != Step::kGPMPasskeySaved) {
    SetCurrentStep(Step::kClosed);
  }
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
  if (use_conditional_mediation_) {
    // Do not show a page-modal retry error sheet if the user cancelled out of
    // their platform authenticator during a conditional UI request.
    // Instead, retry silently.
    CancelAuthenticatorRequest();
    return;
  }

  if (ephemeral_state_.did_dispatch_to_icloud_keychain_) {
    // If we dispatched automatically to iCloud Keychain and the
    // user clicked cancel, give them the option to try something else.
    bool did_trigger_automatically =
        ephemeral_state_.did_invoke_platform_despite_no_priority_mechanism_;
    if (!did_trigger_automatically &&
        model_->priority_mechanism_index.has_value()) {
      const auto& priority_type =
          model_->mechanisms[*model_->priority_mechanism_index].type;
      if (absl::holds_alternative<Mechanism::Credential>(priority_type)) {
        const Mechanism::CredentialInfo* cred_info =
            &absl::get<Mechanism::Credential>(priority_type).value();
        if (cred_info->source == device::AuthenticatorType::kICloudKeychain) {
          did_trigger_automatically = true;
        }
      } else if (absl::holds_alternative<Mechanism::ICloudKeychain>(
                     priority_type)) {
        did_trigger_automatically = true;
      }
    }

    if (did_trigger_automatically) {
      StartOver();
    } else {
      // Otherwise, respect the "Cancel" button in macOS UI as if it were our
      // own.
      CancelAuthenticatorRequest();
    }
    return;
  }
  SetCurrentStep(Step::kErrorInternalUnrecognized);
}

bool AuthenticatorRequestDialogController::OnWinUserCancelled() {
#if BUILDFLAG(IS_WIN)
  if (use_conditional_mediation_) {
    // Do not show a page-modal retry error sheet if the user cancelled out of
    // their platform authenticator during a conditional UI request.
    // Instead, retry silently.
    CancelAuthenticatorRequest();
    return true;
  }

  // If the native Windows API was triggered immediately (i.e. before any Chrome
  // dialog) then start the request over (once) if the user cancels the Windows
  // UI and there are other options in Chrome's UI. But if Windows supports
  // hybrid then we've nothing more to offer in practice.
  if (!WebAuthnApiSupportsHybrid()) {
    bool have_other_option = base::ranges::any_of(
        model_->mechanisms, [](const Mechanism& m) -> bool {
          return absl::holds_alternative<Mechanism::Phone>(m.type) ||
                 absl::holds_alternative<Mechanism::AddPhone>(m.type);
        });
    bool windows_was_priority =
        ephemeral_state_.did_invoke_platform_despite_no_priority_mechanism_ ||
        (model_->priority_mechanism_index &&
         absl::holds_alternative<Mechanism::WindowsAPI>(
             model_->mechanisms[*model_->priority_mechanism_index].type));
    if (have_other_option && windows_was_priority) {
      StartOver();
      return true;
    }
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

void AuthenticatorRequestDialogController::BluetoothAdapterPowerChanged(
    bool powered) {
  model_->ble_adapter_is_powered = powered;
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

void AuthenticatorRequestDialogController::OnHavePIN(std::u16string pin) {
  if (!pin_callback_) {
    // Protect against the view submitting a PIN more than once without
    // receiving a matching response first. |CollectPIN| is called again if
    // the user needs to be prompted for a retry.
    return;
  }
  std::move(pin_callback_).Run(pin);
}

void AuthenticatorRequestDialogController::OnRetryUserVerification(
    int attempts) {
  model_->uv_attempts = attempts;
  SetCurrentStep(Step::kRetryInternalUserVerification);
}

void AuthenticatorRequestDialogController::OnResidentCredentialConfirmed() {
  DCHECK_EQ(model_->step(), Step::kResidentCredentialConfirmation);
  HideDialogAndDispatchToPlatformAuthenticator(
      device::AuthenticatorType::kWinNative);
}

void AuthenticatorRequestDialogController::OnAttestationPermissionResponse(
    bool attestation_permission_granted) {
  if (!attestation_callback_) {
    return;
  }
  std::move(attestation_callback_).Run(attestation_permission_granted);
}

void AuthenticatorRequestDialogController::AddAuthenticator(
    const device::FidoAuthenticator& authenticator) {
  // Only the webauthn.dll authenticator omits a transport completely. This
  // makes sense given how it works, but here it is treated as a platform
  // authenticator and so given a `kInternal` transport.
  DCHECK(authenticator.AuthenticatorTransport() ||
         authenticator.GetType() == device::AuthenticatorType::kWinNative);
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
    model_->creds.emplace_back(device::AuthenticatorType::kOther,
                               model_->relying_party_id,
                               response.credential->id, *response.user_entity);
  }
  selection_callback_ = std::move(callback);
  SetCurrentStep(model_->creds.size() == 1 ? Step::kSelectSingleAccount
                                           : Step::kSelectAccount);
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

device::AuthenticatorType
AuthenticatorRequestDialogController::OnAccountPreselected(
    const std::vector<uint8_t> credential_id) {
  // User selected one of the platform authenticator credentials enumerated in
  // Conditional or regular modal UI prior to collecting user verification.
  // Run `account_preselected_callback_` to narrow the request to the selected
  // credential and dispatch to the platform authenticator.
  const auto cred =
      base::ranges::find_if(transport_availability_.recognized_credentials,
                            [&credential_id](const auto& cred) {
                              return cred.cred_id == credential_id;
                            });
  CHECK(cred != transport_availability_.recognized_credentials.end())
      << "OnAccountPreselected() called with unknown credential_id "
      << base::HexEncode(credential_id);
  const device::AuthenticatorType source = cred->source;
  DCHECK(account_preselected_callback_);
  account_preselected_callback_.Run(*cred);
  model_->creds.clear();
  model_->preselected_cred = *cred;

  if (source != device::AuthenticatorType::kPhone &&
      source != device::AuthenticatorType::kEnclave &&
      source != device::AuthenticatorType::kChromeOSPasskeys) {
    HideDialogAndDispatchToPlatformAuthenticator(source);
    return source;
  }

  const bool use_gpm =
#if BUILDFLAG(IS_CHROMEOS)
      base::FeatureList::IsEnabled(device::kChromeOsPasskeys) ||
#endif
      base::FeatureList::IsEnabled(device::kWebAuthnEnclaveAuthenticator);
  if (use_gpm) {
    model_->OnGPMPasskeySelected(credential_id);
    return source;
  }

  ContactPriorityPhone();
  return source;
}

void AuthenticatorRequestDialogController::OnAccountPreselectedIndex(
    size_t index) {
  OnAccountPreselected(model_->creds.at(index).cred_id);
}

void AuthenticatorRequestDialogController::SetSelectedAuthenticatorForTesting(
    AuthenticatorReference test_authenticator) {
  ephemeral_state_.selected_authenticator_id_ =
      test_authenticator.authenticator_id;
  ephemeral_state_.saved_authenticators_.AddAuthenticator(
      std::move(test_authenticator));
}

void AuthenticatorRequestDialogController::ContactPriorityPhone() {
  ContactPhone(paired_phones_[*priority_phone_index_]->name);
}

void AuthenticatorRequestDialogController::ContactPhoneForTesting(
    const std::string& name) {
  // Ensure BLE is powered so that `ContactPhone()` shows the "Check your phone"
  // screen right away.
  model_->ble_adapter_is_powered = true;
  ContactPhone(name);
}

void AuthenticatorRequestDialogController::SetPriorityPhoneIndex(
    std::optional<size_t> index) {
  if (index) {
    model_->priority_phone_name = paired_phones_.at(*index)->name;
  } else {
    model_->priority_phone_name.reset();
  }
  priority_phone_index_ = index;
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

void AuthenticatorRequestDialogController::OnBioEnrollmentDone() {
  std::move(bio_enrollment_callback_).Run();
}

void AuthenticatorRequestDialogController::RequestAttestationPermission(
    bool is_enterprise_attestation,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(model_->step() != Step::kClosed);
  attestation_callback_ = std::move(callback);
  SetCurrentStep(is_enterprise_attestation
                     ? Step::kEnterpriseAttestationPermissionRequest
                     : Step::kAttestationPermissionRequest);
}

void AuthenticatorRequestDialogController::set_cable_transport_info(
    std::optional<bool> extension_is_v2,
    std::vector<std::unique_ptr<device::cablev2::Pairing>> paired_phones,
    base::RepeatingCallback<void(std::unique_ptr<device::cablev2::Pairing>)>
        contact_phone_callback,
    const std::optional<std::string>& cable_qr_string) {
  DCHECK(paired_phones.empty() || contact_phone_callback);

  if (extension_is_v2.has_value()) {
    cable_extension_provided_ = true;
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

  paired_phones_ = std::move(paired_phones);
  contact_phone_callback_ = std::move(contact_phone_callback);
  model_->cable_qr_string = cable_qr_string;

  model_->paired_phone_names.clear();
  base::ranges::transform(paired_phones_,
                          std::back_inserter(model_->paired_phone_names),
                          &device::cablev2::Pairing::name);
  model_->paired_phone_names.erase(
      std::unique(model_->paired_phone_names.begin(),
                  model_->paired_phone_names.end()),
      model_->paired_phone_names.end());

  paired_phones_contacted_.assign(paired_phones_.size(), false);
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
          device::FidoRequestType::kMakeCredential &&
      transport_availability_.make_credential_attachment.has_value() &&
      *transport_availability_.make_credential_attachment ==
          device::AuthenticatorAttachment::kPlatform) {
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
             !use_conditional_mediation_) {
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
    device::FidoRequestType request_type,
    device::AuthenticatorType authenticator_type) {
  if (!did_record_macos_start_histogram_) {
    return;
  }

  std::optional<MacOsHistogramValues> v;

  if (transport_availability_.request_type ==
      device::FidoRequestType::kMakeCredential) {
    if (authenticator_type == device::AuthenticatorType::kTouchID) {
      v = has_icloud_drive_enabled_
              ? MacOsHistogramValues::
                    kSuccessfulCreateForProfileAuthenticatorICloudDriveEnabled
              : MacOsHistogramValues::
                    kSuccessfulCreateForProfileAuthenticatorICloudDriveDisabled;
    } else if (authenticator_type ==
               device::AuthenticatorType::kICloudKeychain) {
      v = has_icloud_drive_enabled_
              ? MacOsHistogramValues::
                    kSuccessfulCreateForICloudKeychainICloudDriveEnabled
              : MacOsHistogramValues::
                    kSuccessfulCreateForICloudKeychainICloudDriveDisabled;
    }
  } else {
    if (authenticator_type == device::AuthenticatorType::kTouchID) {
      v = MacOsHistogramValues::kSuccessfulGetFromProfileAuthenticator;
    } else if (authenticator_type ==
               device::AuthenticatorType::kICloudKeychain) {
      v = MacOsHistogramValues::kSuccessfulGetFromICloudKeychain;
    }
  }

  if (v) {
    base::UmaHistogramEnumeration(
        "WebAuthentication.MacOS.PlatformAuthenticatorAction", *v);
  }
}

void AuthenticatorRequestDialogController::
    set_is_active_profile_authenticator_user(bool is_active) {
  is_active_profile_authenticator_user_ = is_active;
}

void AuthenticatorRequestDialogController::set_has_icloud_drive_enabled(
    bool is_enabled) {
  has_icloud_drive_enabled_ = is_enabled;
}

#endif

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
         model_->step() == Step::kAndroidAccessory ||
         model_->step() == Step::kConditionalMediation ||
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
      EnsureBleAdapterIsPoweredAndContinueWithStep(Step::kCableActivate);
      break;
    case AuthenticatorTransport::kAndroidAccessory:
      SetCurrentStep(Step::kAndroidAccessory);
      break;
    default:
      break;
  }
}

void AuthenticatorRequestDialogController::StartGuidedFlowForAddPhone() {
  EnsureBleAdapterIsPoweredAndContinueWithStep(Step::kCableV2QRCode);
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
    HideDialogAndDispatchToPlatformAuthenticator(
        device::AuthenticatorType::kWinNative);
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
      if (cred.source == device::AuthenticatorType::kICloudKeychain) {
        selected = &cred;
        break;
      }
    }
    account_preselected_callback_.Run(*selected);
  }

  HideDialogAndDispatchToPlatformAuthenticator(
      device::AuthenticatorType::kICloudKeychain);
}

void AuthenticatorRequestDialogController::StartEnclave() {
  model_->OnGPMSelected();
}

void AuthenticatorRequestDialogController::ReauthForSyncRestore() {
  signin_ui_util::ShowReauthForPrimaryAccountWithAuthError(
      Profile::FromBrowserContext(
          model_->GetWebContents()->GetBrowserContext()),
      signin_metrics::AccessPoint::ACCESS_POINT_WEBAUTHN_MODAL_DIALOG);
  CancelAuthenticatorRequest();
}

void AuthenticatorRequestDialogController::ContactPhone(
    const std::string& name) {
#if BUILDFLAG(IS_MAC)
  if (transport_availability()->ble_access_denied) {
    // |step| is not saved because macOS asks the user to restart Chrome
    // after permission has been granted. So the user will end up retrying
    // the whole WebAuthn request in the new process.
    SetCurrentStep(Step::kBlePermissionMac);
    return;
  }
#endif

  if (transport_availability_.request_type ==
          device::FidoRequestType::kMakeCredential &&
      transport_availability_.is_off_the_record_context) {
    after_off_the_record_interstitial_ =
        base::BindOnce(&AuthenticatorRequestDialogController::
                           ContactPhoneAfterOffTheRecordInterstitial,
                       weak_factory_.GetWeakPtr(), name);
    SetCurrentStep(Step::kOffTheRecordInterstitial);
    return;
  }

  ContactPhoneAfterOffTheRecordInterstitial(name);
}

void AuthenticatorRequestDialogController::
    ContactPhoneAfterOffTheRecordInterstitial(std::string name) {
  if (!model_->ble_adapter_is_powered) {
    after_ble_adapter_powered_ = base::BindOnce(
        &AuthenticatorRequestDialogController::ContactPhoneAfterBleIsPowered,
        weak_factory_.GetWeakPtr(), std::move(name));

    if (transport_availability()->can_power_on_ble_adapter) {
      SetCurrentStep(Step::kBlePowerOnAutomatic);
    } else {
      SetCurrentStep(Step::kBlePowerOnManual);
    }
    return;
  }

  ContactPhoneAfterBleIsPowered(std::move(name));
}

void AuthenticatorRequestDialogController::ContactPhoneAfterBleIsPowered(
    std::string name) {
  ContactNextPhoneByName(name);
  SetCurrentStep(Step::kCableActivate);
}

void AuthenticatorRequestDialogController::StartConditionalMediationRequest() {
  model_->creds = transport_availability_.recognized_credentials;

  auto* render_frame_host = model_->GetRenderFrameHost();
  auto* web_contents = model_->GetWebContents();
  if (web_contents && render_frame_host) {
    std::vector<password_manager::PasskeyCredential> credentials;
    std::optional<size_t> priority_phone_index =
        GetIndexOfMostRecentlyUsedPhoneFromSync();
    std::optional<std::u16string> priority_phone_name;
    if (priority_phone_index) {
      priority_phone_name =
          base::UTF8ToUTF16(paired_phones_[*priority_phone_index]->name);
    }
    for (const auto& credential : model_->creds) {
      if (credential.source == device::AuthenticatorType::kPhone &&
          !priority_phone_index) {
        continue;
      }
      if (credential.source == device::AuthenticatorType::kEnclave &&
          !enclave_enabled_) {
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
      if (credential.source == device::AuthenticatorType::kPhone) {
        passkey.set_authenticator_label(l10n_util::GetStringFUTF16(
            IDS_PASSWORD_MANAGER_PASSKEY_FROM_PHONE, *priority_phone_name));
      }
    }
    bool offer_passkey_from_another_device;
    switch (transport_availability_.conditional_ui_treatment) {
      case TransportAvailabilityInfo::ConditionalUITreatment::kDefault:
        offer_passkey_from_another_device = true;
        break;
      case TransportAvailabilityInfo::ConditionalUITreatment::
          kDontShowEmptyConditionalUI:
        offer_passkey_from_another_device = !credentials.empty();
        break;
      case TransportAvailabilityInfo::ConditionalUITreatment::
          kNeverOfferPasskeyFromAnotherDevice:
        offer_passkey_from_another_device = false;
        break;
    }
    ReportConditionalUiPasskeyCount(credentials.size());
    auto* webauthn_credentials_delegate_factory =
        ChromeWebAuthnCredentialsDelegateFactory::GetFactory(web_contents)
            ->GetDelegateForFrame(render_frame_host);
    if (webauthn_credentials_delegate_factory) {
      // May be null on tests.
      webauthn_credentials_delegate_factory->OnCredentialsReceived(
          std::move(credentials), offer_passkey_from_another_device);
    }
  }
  SetCurrentStep(Step::kConditionalMediation);
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

void AuthenticatorRequestDialogController::ContactNextPhoneByName(
    const std::string& name) {
  bool found_name = false;
  model_->selected_phone_name.reset();
  for (size_t i = 0; i != paired_phones_.size(); i++) {
    const std::unique_ptr<device::cablev2::Pairing>& phone = paired_phones_[i];
    if (phone->name == name) {
      found_name = true;
      model_->selected_phone_name = name;
      if (!paired_phones_contacted_[i]) {
        MaybeStoreLastUsedPairing(model_->GetRenderFrameHost(),
                                  phone->peer_public_key_x962);
        paired_phones_contacted_[i] = true;
        contact_phone_callback_.Run(
            std::make_unique<device::cablev2::Pairing>(*phone));
        break;
      }
    } else if (found_name) {
      // |paired_phones_| is sorted by name so as soon as we see a mismatch
      // after a match, we're done.
      break;
    }
  }

  DCHECK(found_name);
}

std::optional<size_t>
AuthenticatorRequestDialogController::GetIndexOfMostRecentlyUsedPhoneFromSync()
    const {
  // Try finding the most recently used phone from sync.
  std::optional<std::vector<uint8_t>> last_used_pairing =
      RetrieveLastUsedPairing(model_->GetRenderFrameHost());
  if (last_used_pairing) {
    for (size_t i = 0; i < paired_phones_.size(); ++i) {
      if (paired_phones_[i]->from_sync_deviceinfo &&
          base::ranges::equal(paired_phones_[i]->peer_public_key_x962,
                              *last_used_pairing)) {
        return i;
      }
    }
  }
  // Could not find a most recently used phone. Instead, return the phone that
  // last published to sync.
  std::optional<int> ret;
  for (size_t i = 0; i < paired_phones_.size(); ++i) {
    if (paired_phones_[i]->from_sync_deviceinfo) {
      if (!ret || paired_phones_[*ret]->last_updated <
                      paired_phones_[i]->last_updated) {
        ret = i;
      }
    }
  }
  return ret;
}

void AuthenticatorRequestDialogController::SortRecognizedCredentials() {
  struct {
    bool operator()(const device::DiscoverableCredentialMetadata& a,
                    const device::DiscoverableCredentialMetadata& b) {
      return std::tie(a.user.id, a.cred_id) < std::tie(b.user.id, b.cred_id);
    }
  } id_comparator;
  base::ranges::sort(transport_availability_.recognized_credentials,
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

  base::ranges::stable_sort(transport_availability_.recognized_credentials,
                            std::ref(user_name_comparator));
}

void AuthenticatorRequestDialogController::PopulateMechanisms() {
  const bool is_get_assertion = transport_availability_.request_type ==
                                device::FidoRequestType::kGetAssertion;
  SetPriorityPhoneIndex(GetIndexOfMostRecentlyUsedPhoneFromSync());
  bool list_phone_passkeys =
      is_get_assertion && priority_phone_index_ &&
      base::FeatureList::IsEnabled(syncer::kSyncWebauthnCredentials);
  bool specific_phones_listed = false;
  bool specific_local_passkeys_listed = false;
  if (is_get_assertion && !use_conditional_mediation_) {
    // List passkeys instead of mechanisms for platform & GPM authenticators.
    for (const auto& cred : transport_availability_.recognized_credentials) {
      if (cred.source == device::AuthenticatorType::kPhone &&
          !list_phone_passkeys) {
        continue;
      }
      if (cred.source == device::AuthenticatorType::kICloudKeychain &&
          !allow_icloud_keychain_) {
        continue;
      }
      if (cred.source == device::AuthenticatorType::kEnclave &&
          enclave_needs_reauth_) {
        // Do not list passkeys from the enclave if it needs reauth before
        // proceeding.  Instead, we'll show a button to trigger reauth.
        continue;
      }
      if (cred.source == device::AuthenticatorType::kPhone) {
        specific_phones_listed = true;
      } else {
        specific_local_passkeys_listed = true;
      }
      std::u16string name = base::UTF8ToUTF16(cred.user.name.value_or(""));
      auto& mechanism = model_->mechanisms.emplace_back(
          AuthenticatorRequestDialogModel::Mechanism::Credential(
              {cred.source, cred.user.id}),
          name, name, GetCredentialIcon(cred.source),
          base::BindRepeating(
              base::IgnoreResult(
                  &AuthenticatorRequestDialogController::OnAccountPreselected),
              base::Unretained(this), cred.cred_id));
      mechanism.description =
          GetMechanismDescription(cred.source, model_->priority_phone_name);
    }
  }

  std::vector<AuthenticatorTransport> transports_to_list_if_active;
  // Do not list the internal transport if we can offer users to select a
  // platform credential directly. This is true for both conditional requests
  // and the new passkey selector UI.
  bool did_enumerate_local_passkeys = false;
  if (use_conditional_mediation_) {
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
        transports_to_list_if_active.push_back(
            AuthenticatorTransport::kAndroidAccessory);
        [[fallthrough]];

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

  if (base::FeatureList::IsEnabled(device::kWebAuthnEnclaveAuthenticator) &&
      enclave_enabled_ && !is_get_assertion &&
      *transport_availability_.make_credential_attachment !=
          device::AuthenticatorAttachment::kCrossPlatform) {
    const std::u16string name =
        l10n_util::GetStringUTF16(IDS_WEBAUTHN_SOURCE_GOOGLE_PASSWORD_MANAGER);
    model_->mechanisms.emplace_back(
        Mechanism::Enclave(), name, name, vector_icons::kPasswordManagerIcon,
        base::BindRepeating(&AuthenticatorRequestDialogController::StartEnclave,
                            base::Unretained(this)));
  }
  if (enclave_needs_reauth_ && !use_conditional_mediation_) {
    // Show a button that lets the user sign in again to restore sync. This
    // cancels the request, so we can't do it for conditional UI requests.
    // TODO(enclave): add support for conditional UI.
    const std::u16string name =
        l10n_util::GetStringUTF16(IDS_WEBAUTHN_SIGN_IN_AGAIN_TITLE);
    Mechanism enclave(
        Mechanism::Enclave(), name, name, vector_icons::kSyncIcon,
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
       transport_availability_.has_icloud_keychain_credential ==
           device::FidoRequestHandlerBase::RecognizedCredential::kUnknown)) {
    const std::u16string name =
        l10n_util::GetStringUTF16(IDS_WEBAUTHN_TRANSPORT_ICLOUD_KEYCHAIN);
    model_->mechanisms.emplace_back(
        Mechanism::ICloudKeychain(), name, name, kIcloudKeychainIcon,
        base::BindRepeating(
            &AuthenticatorRequestDialogController::StartICloudKeychain,
            base::Unretained(this)));
  }

  std::optional<std::pair<int, AuthenticatorTransport>> windows_button_label;
  windows_button_label =
      GetWindowsAPIButtonLabel(transport_availability_, specific_phones_listed);
  if (windows_button_label &&
      windows_button_label->second == AuthenticatorTransport::kInternal) {
    // Add the Windows button before phones if it can trigger Windows Hello.
    AddWindowsButton(windows_button_label->first, windows_button_label->second);
  }

  // Only list phones as transports if we did not already list GPM passkeys
  // above and this is an allow-list request. That way, users can tap their
  // synced phone name to use a non-discoverable credential from their synced
  // phone.
  bool all_matching_phone_creds_listed =
      list_phone_passkeys &&
      (specific_phones_listed || transport_availability_.has_empty_allow_list);
  if (base::Contains(transport_availability_.available_transports, kCable) &&
      !all_matching_phone_creds_listed && !windows_handles_hybrid) {
    // List phones as transports.
    for (const auto& phone_name : model_->paired_phone_names) {
      const std::u16string name16 = base::UTF8ToUTF16(phone_name);
      static constexpr size_t kMaxLongNameChars = 50;
      static constexpr size_t kMaxShortNameChars = 30;
      std::u16string long_name, short_name;
      gfx::ElideString(name16, kMaxLongNameChars, &long_name);
      gfx::ElideString(name16, kMaxShortNameChars, &short_name);

      model_->mechanisms.emplace_back(
          Mechanism::Phone(phone_name), std::move(long_name),
          std::move(short_name), kSmartphoneIcon,
          base::BindRepeating(
              &AuthenticatorRequestDialogController::ContactPhone,
              base::Unretained(this), phone_name));
      specific_phones_listed = true;
    }
    bool skip_to_phone_confirmation =
        is_get_assertion &&
#if BUILDFLAG(IS_WIN)
        !WebAuthnApiSupportsHybrid() &&
#endif
        transport_availability_.has_platform_authenticator_credential ==
            device::FidoRequestHandlerBase::RecognizedCredential::
                kNoRecognizedCredential &&
        transport_availability_.has_icloud_keychain_credential ==
            device::FidoRequestHandlerBase::RecognizedCredential::
                kNoRecognizedCredential &&
        paired_phones_.size() == 1 && !use_conditional_mediation_ &&
        transport_availability_.is_only_hybrid_or_internal;
    if (skip_to_phone_confirmation) {
      SetPriorityPhoneIndex(0);
      pending_step_ = Step::kPhoneConfirmationSheet;
    }
  }

  // If the new UI is enabled, only show USB as an option if the QR code is
  // not available, if tapping it would trigger a prompt to enable BLE, or if
  // hints will cause us to jump to USB UI.
  const bool include_usb_option =
      base::Contains(transport_availability_.available_transports,
                     AuthenticatorTransport::kUsbHumanInterfaceDevice) &&
      (!include_add_phone_option || !model_->ble_adapter_is_powered ||
       transport_availability_.ble_access_denied ||
       hints_.transport == AuthenticatorTransport::kUsbHumanInterfaceDevice);

  if (include_add_phone_option) {
    std::u16string label = l10n_util::GetStringUTF16(
        GetHybridButtonLabel(!include_usb_option, specific_phones_listed));
    model_->mechanisms.emplace_back(
        Mechanism::AddPhone(), label, label, kQrcodeGeneratorIcon,
        base::BindRepeating(
            &AuthenticatorRequestDialogController::StartGuidedFlowForAddPhone,
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

    model_->mechanisms.emplace_back(
        Mechanism::Transport(transport), GetTransportDescription(transport),
        GetTransportShortDescription(transport), GetTransportIcon(transport),
        base::BindRepeating(
            &AuthenticatorRequestDialogController::StartGuidedFlowForTransport,
            base::Unretained(this), transport));
  }
  // Add the Windows native API button last if it does not do Windows Hello.
  if (windows_button_label &&
      windows_button_label->second != AuthenticatorTransport::kInternal) {
    AddWindowsButton(windows_button_label->first, windows_button_label->second);
  }
}

void AuthenticatorRequestDialogController::AddWindowsButton(
    int label,
    AuthenticatorTransport transport) {
  const std::u16string desc = l10n_util::GetStringUTF16(label);
  model_->mechanisms.emplace_back(
      Mechanism::WindowsAPI(), desc, desc, GetTransportIcon(transport),
      base::BindRepeating(
          &AuthenticatorRequestDialogController::StartWinNativeApi,
          base::Unretained(this)));
}

std::optional<size_t>
AuthenticatorRequestDialogController::IndexOfPriorityMechanism() {
  // Never pick a priority mechanism if we are showing the enclave reauth
  // button.
  if (enclave_needs_reauth_ && !use_conditional_mediation_) {
    return std::nullopt;
  }
  if (transport_availability_.request_type ==
      device::FidoRequestType::kGetAssertion) {
    // If there is a single mechanism, go to that.
    if (model_->mechanisms.size() == 1) {
      return 0;
    }

    if (transport_availability_.has_empty_allow_list) {
      // The index and info of the credential that the UI should default to.
      std::optional<std::pair<size_t, const Mechanism::CredentialInfo*>>
          best_cred;
      bool multiple_distinct_creds = false;

      for (size_t i = 0; i < model_->mechanisms.size(); ++i) {
        const auto& type = model_->mechanisms[i].type;
        if (absl::holds_alternative<Mechanism::Credential>(type)) {
          const Mechanism::CredentialInfo* cred_info =
              &absl::get<Mechanism::Credential>(type).value();

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
        }
      }
      // If one of the passkeys is a valid default, go to that.
      if (!multiple_distinct_creds && best_cred.has_value()) {
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

  if (model_->mechanisms.size() == 1) {
    return 0;
  } else if (model_->mechanisms.empty()) {
    return std::nullopt;
  }

  bool windows_handles_hybrid = WebAuthnApiSupportsHybrid();
  std::vector<Mechanism::Type> priority_list;

  if (transport_availability_.request_type ==
      device::FidoRequestType::kGetAssertion) {
    const bool is_passkey_request =
        transport_availability_.has_empty_allow_list ||
        transport_availability_.is_only_hybrid_or_internal;
    if (!use_conditional_mediation_) {
      // The following is moot in practice if `windows_handles_hybrid` because,
      // in that situation, neither an `internal` transport nor iCloud Keychain
      // will be available. But this simplifies unittests.
      if (!windows_handles_hybrid) {
        // If there's a match on the platform authenticator, jump to that.
        if (transport_availability_.has_icloud_keychain_credential ==
                device::FidoRequestHandlerBase::RecognizedCredential::
                    kHasRecognizedCredential &&
            allow_icloud_keychain_) {
          priority_list.emplace_back(Mechanism::ICloudKeychain());
        }
        if (transport_availability_.has_platform_authenticator_credential ==
            device::FidoRequestHandlerBase::RecognizedCredential::
                kHasRecognizedCredential) {
          priority_list.emplace_back(
              Mechanism::Transport(AuthenticatorTransport::kInternal));
        }
      }

      // If it's caBLEv1, or server-linked caBLEv2, jump to that.
      if (model_->cable_ui_type) {
        switch (*model_->cable_ui_type) {
          case AuthenticatorRequestDialogModel::CableUIType::
              CABLE_V2_SERVER_LINK:
          case AuthenticatorRequestDialogModel::CableUIType::CABLE_V1:
            priority_list.emplace_back(
                Mechanism::Transport(AuthenticatorTransport::kHybrid));
            break;
          case AuthenticatorRequestDialogModel::CableUIType::
              CABLE_V2_2ND_FACTOR:
            break;
        }
      }

      // This seems like it might be an error (crbug.com/1426243): kInternal has
      // priority over caBLE extensions if there's a recognised platform
      // credential, but Windows doesn't.
      if (transport_availability_.has_platform_authenticator_credential ==
          device::FidoRequestHandlerBase::RecognizedCredential::
              kHasRecognizedCredential) {
        priority_list.emplace_back(Mechanism::WindowsAPI());
      }

      // Prefer going straight to Windows native UI for requests that are not
      // clearly passkeys related,
      if (!is_passkey_request) {
        priority_list.emplace_back(Mechanism::WindowsAPI());
      }
    }

    if (windows_handles_hybrid) {
      priority_list.emplace_back(Mechanism::WindowsAPI());
    }

    if (is_passkey_request && model_->paired_phone_names.empty() &&
        // On Windows WebAuthn API < 4, we cannot tell in advance if the
        // platform authenticator can fulfill a get assertion request. In that
        // case, don't jump to the QR code.
        (use_conditional_mediation_ ||
         transport_availability_.has_platform_authenticator_credential ==
             device::FidoRequestHandlerBase::RecognizedCredential::
                 kNoRecognizedCredential)) {
      priority_list.emplace_back(Mechanism::AddPhone());
    }
  } else {
    CHECK_EQ(transport_availability_.request_type,
             device::FidoRequestType::kMakeCredential);

    Profile* profile = Profile::FromBrowserContext(
                           model_->GetRenderFrameHost()->GetBrowserContext())
                           ->GetOriginalProfile();
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
    if (base::FeatureList::IsEnabled(device::kWebAuthnEnclaveAuthenticator) &&
        !enclave_decline_limit_reached && !enclave_bootstrap_limit_reached &&
        enclave_enabled_ &&
        *transport_availability_.make_credential_attachment ==
            device::AuthenticatorAttachment::kPlatform) {
      priority_list.emplace_back(Mechanism::Enclave());
    }

    if (windows_handles_hybrid) {
      // If Windows supports hybrid and the enclave is not available, we defer
      // to the platform.
      const bool enclave_available = base::ranges::any_of(
          model_->mechanisms, [](const Mechanism& m) -> bool {
            return absl::holds_alternative<Mechanism::Enclave>(m.type);
          });
      if (!enclave_available || enclave_decline_limit_reached) {
        priority_list.emplace_back(Mechanism::WindowsAPI());
      }
    }

#if BUILDFLAG(IS_MAC)
    if (transport_availability_.make_credential_attachment ==
        device::AuthenticatorAttachment::kPlatform) {
      // For platform attachments, either we have iCloud Keychain available
      // or not. If not, then there's only a single active mechanism (the
      // profile authenticator) and we would have picked it above. Thus here we
      // must be picking between the profile authenticator and iCloud Keychain.
      if (should_create_in_icloud_keychain_) {
        priority_list.emplace_back(Mechanism::ICloudKeychain());
      } else {
        priority_list.emplace_back(
            Mechanism::Transport(AuthenticatorTransport::kInternal));
      }
    }
#endif

    // If attachment=any, then don't jump to suggesting any specific mechanism.
    if (*transport_availability_.make_credential_attachment !=
        device::AuthenticatorAttachment::kAny) {
      const bool is_passkey_request =
          model_->resident_key_requirement !=
          device::ResidentKeyRequirement::kDiscouraged;
      if (is_passkey_request) {
        if (model_->paired_phone_names.empty()) {
          priority_list.emplace_back(Mechanism::AddPhone());
        }
      } else {
        // This seems like it might be an error (crbug.com/1426244) as we might
        // still want to jump to platform authenticators for passkey requests if
        // we don't jump to a phone.
        priority_list.emplace_back(Mechanism::WindowsAPI());
      }
    }
  }

  for (const auto& priority_mechanism : priority_list) {
    // A phone should never be triggered immediately.
    CHECK(!absl::holds_alternative<Mechanism::Phone>(priority_mechanism));

    for (size_t i = 0; i < model_->mechanisms.size(); i++) {
      if (priority_mechanism == model_->mechanisms[i].type) {
        return i;
      }
    }
  }

  return std::nullopt;
}

void AuthenticatorRequestDialogController::OnPasskeysChanged(
    const std::vector<webauthn::PasskeyModelChange>& changes) {
  if (model_->step() != Step::kConditionalMediation) {
    // Updating an in flight request is only supported for conditional UI.
    return;
  }

  // If the user just opted in to sync, it is likely the hybrid discovery needs
  // to be reconfigured for a newly synced down phone. Start the request over to
  // give the request delegate a chance to do this.
  for (auto& observer : model_->observers) {
    observer.OnStartOver();
  }
}

void AuthenticatorRequestDialogController::OnPasskeyModelShuttingDown() {
  passkey_model_observation_.Reset();
}

void AuthenticatorRequestDialogController::
    UpdateModelForTransportAvailability() {
  model_->request_type = transport_availability_.request_type;
  model_->resident_key_requirement =
      transport_availability_.resident_key_requirement;
  model_->ble_adapter_is_powered = transport_availability_.is_ble_powered;
  model_->security_key_is_possible =
      base::Contains(transport_availability_.available_transports,
                     device::FidoTransportProtocol::kUsbHumanInterfaceDevice);
  model_->is_off_the_record = transport_availability_.is_off_the_record_context;
  if (model_->cable_ui_type) {
    model_->cable_should_suggest_usb =
        *model_->cable_ui_type !=
            AuthenticatorRequestDialogModel::CableUIType::CABLE_V1 &&
        base::Contains(transport_availability_.available_transports,
                       AuthenticatorTransport::kAndroidAccessory);
  }
}

void AuthenticatorRequestDialogController::OnUserConfirmedPriorityMechanism() {
  model_->mechanisms[*model_->priority_mechanism_index].callback.Run();
}

void AuthenticatorRequestDialogController::
    HideDialogAndDispatchToPlatformAuthenticator(
        std::optional<device::AuthenticatorType> type) {
  HideDialog();

  std::vector<AuthenticatorReference>& authenticators =
      ephemeral_state_.saved_authenticators_.authenticator_list();
#if BUILDFLAG(IS_WIN)
  // The Windows-native UI already handles retrying so we do not offer a second
  // level of retry in that case.
  if (type && *type != device::AuthenticatorType::kEnclave) {
    model_->offer_try_again_in_ui = false;
  }
#elif BUILDFLAG(IS_MAC)
  // If there are multiple platform authenticators, one of them is the default.
  if (!type.has_value() &&
      base::FeatureList::IsEnabled(
          device::kWebAuthnPreferVirtualPlatformAuthenticator)) {
    if (base::ranges::any_of(
            authenticators, [](const AuthenticatorReference& ref) {
              return ref.type == device::AuthenticatorType::kOther &&
                     ref.transport == device::FidoTransportProtocol::kInternal;
            })) {
      type = device::AuthenticatorType::kOther;
    }
  }

  if (!type.has_value()) {
    type = device::AuthenticatorType::kTouchID;
  }
#endif

  auto platform_authenticator_it = base::ranges::find_if(
      authenticators, [type](const AuthenticatorReference& ref) -> bool {
        if (type && *type == device::AuthenticatorType::kEnclave) {
          return ref.type == *type;
        }
        return ref.transport == device::FidoTransportProtocol::kInternal &&
               (!type || ref.type == *type);
      });

  if (platform_authenticator_it == authenticators.end()) {
    return;
  }

  if (platform_authenticator_it->type ==
      device::AuthenticatorType::kICloudKeychain) {
    ephemeral_state_.did_dispatch_to_icloud_keychain_ = true;
  }

  DispatchRequestAsync(&*platform_authenticator_it);
}

void AuthenticatorRequestDialogController::OnCreatePasskeyAccepted() {
  HideDialogAndDispatchToPlatformAuthenticator();
}

void AuthenticatorRequestDialogController::EnclaveEnabled() {
  enclave_enabled_ = true;
}

void AuthenticatorRequestDialogController::EnclaveNeedsReauth() {
  enclave_needs_reauth_ = true;
}

void AuthenticatorRequestDialogController::OnTransportAvailabilityChanged(
    TransportAvailabilityInfo transport_availability) {
  if (model_->step() != Step::kConditionalMediation) {
    // Updating an in flight request is only supported for conditional UI.
    return;
  }
  transport_availability_ = std::move(transport_availability);
  UpdateModelForTransportAvailability();
  SortRecognizedCredentials();
  model_->mechanisms.clear();
  PopulateMechanisms();
  model_->priority_mechanism_index = IndexOfPriorityMechanism();
  StartConditionalMediationRequest();
}
