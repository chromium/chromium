// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"

#include <algorithm>
#include <iterator>
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
#include "chrome/browser/ui/webauthn/authenticator_request_dialog.h"
#include "chrome/browser/webauthn/authenticator_transport.h"
#include "chrome/browser/webauthn/webauthn_metrics_util.h"
#include "chrome/browser/webauthn/webauthn_pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/features.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/pin.h"
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
#include "device/fido/mac/util.h"
#endif

namespace {

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
      return vector_icons::kUsbIcon;
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
    case device::AuthenticatorType::kChromeOS:
    case device::AuthenticatorType::kEnclave:
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
absl::optional<std::vector<uint8_t>> RetrieveLastUsedPairing(
    content::RenderFrameHost* rfh) {
  if (!rfh) {
    // The RFH might be null in unit tests, or it might not be alive anymore.
    return absl::nullopt;
  }
  Profile* profile = Profile::FromBrowserContext(rfh->GetBrowserContext());
  std::string maybe_last_used_pairing = profile->GetPrefs()->GetString(
      webauthn::pref_names::kLastUsedPairingFromSyncPublicKey);
  absl::optional<std::vector<uint8_t>> last_used_pairing;
  if (maybe_last_used_pairing.empty()) {
    return absl::nullopt;
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
    const absl::optional<std::u16string>& priority_phone_name) {
  if (type == device::AuthenticatorType::kPhone) {
    return l10n_util::GetStringFUTF16(IDS_WEBAUTHN_SOURCE_PHONE,
                                      *priority_phone_name);
  }
  int message;
  switch (type) {
    case device::AuthenticatorType::kWinNative:
      message = IDS_WEBAUTHN_SOURCE_WINDOWS_HELLO;
      break;
    case device::AuthenticatorType::kTouchID:
      message = IDS_WEBAUTHN_SOURCE_CHROME_PROFILE;
      break;
    case device::AuthenticatorType::kICloudKeychain:
      // TODO(crbug.com/1439987): Use IDS_WEBAUTHN_SOURCE_CUSTOM_VENDOR for
      // third party providers.
      message = IDS_WEBAUTHN_SOURCE_ICLOUD_KEYCHAIN;
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
// triggers the Windows native WebAuthn API, or absl::nullopt if the button
// should not be shown. The transport represents the option Windows will prefer
// when tapping the button and is used to pick an icon and position on the list.
absl::optional<std::pair<int, AuthenticatorTransport>> GetWindowsAPIButtonLabel(
    const device::FidoRequestHandlerBase::TransportAvailabilityInfo&
        transport_availability,
    bool specific_phones_listed) {
  if (!transport_availability.has_win_native_api_authenticator) {
    return absl::nullopt;
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
  return absl::nullopt;
}

}  // namespace

AuthenticatorRequestDialogModel::EphemeralState::EphemeralState() = default;
AuthenticatorRequestDialogModel::EphemeralState::EphemeralState(
    EphemeralState&&) = default;
AuthenticatorRequestDialogModel::EphemeralState&
AuthenticatorRequestDialogModel::EphemeralState::operator=(EphemeralState&&) =
    default;
AuthenticatorRequestDialogModel::EphemeralState::~EphemeralState() = default;

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

void AuthenticatorRequestDialogModel::ResetEphemeralState() {
  ephemeral_state_ = {};
}

AuthenticatorRequestDialogModel::AuthenticatorRequestDialogModel(
    content::RenderFrameHost* frame_host) {
  if (frame_host) {
    frame_host_id_ = frame_host->GetGlobalId();
  }
}

AuthenticatorRequestDialogModel::~AuthenticatorRequestDialogModel() {
  for (auto& observer : observers_) {
    observer.OnModelDestroyed(this);
  }
}

void AuthenticatorRequestDialogModel::HideDialog() {
  SetCurrentStep(Step::kNotStarted);
}

void AuthenticatorRequestDialogModel::StartFlow(
    TransportAvailabilityInfo transport_availability,
    bool use_conditional_mediation) {
  DCHECK(!started_);
  DCHECK_EQ(current_step(), Step::kNotStarted);

  started_ = true;
  transport_availability_ = std::move(transport_availability);
  use_conditional_mediation_ = use_conditional_mediation;

  if (base::FeatureList::IsEnabled(
          device::kWebAuthnSortRecognizedCredentials)) {
    SortRecognizedCredentials();
  }

#if BUILDFLAG(IS_MAC)
  RecordMacOsStartedHistogram();
#endif

  PopulateMechanisms();
  ephemeral_state_.priority_mechanism_index_ = IndexOfPriorityMechanism();

  if (use_conditional_mediation_) {
    // This is a conditional mediation request.
    StartConditionalMediationRequest();
  } else {
    StartGuidedFlowForMostLikelyTransportOrShowMechanismSelection();
  }
}

void AuthenticatorRequestDialogModel::StartOver() {
  ResetEphemeralState();

  for (auto& observer : observers_) {
    observer.OnStartOver();
  }

  current_step_ = Step::kNotStarted;
  SetCurrentStep(Step::kMechanismSelection);
}

void AuthenticatorRequestDialogModel::TransitionToModalWebAuthnRequest() {
  DCHECK_EQ(current_step(), Step::kConditionalMediation);

  // Dispatch requests to any plugged in authenticators.
  for (auto& authenticator :
       ephemeral_state_.saved_authenticators_.authenticator_list()) {
    if (authenticator.transport != device::FidoTransportProtocol::kInternal) {
      DispatchRequestAsync(&authenticator);
    }
  }
  StartGuidedFlowForMostLikelyTransportOrShowMechanismSelection();
}

void AuthenticatorRequestDialogModel::
    StartGuidedFlowForMostLikelyTransportOrShowMechanismSelection() {
  if (pending_step_) {
    SetCurrentStep(*pending_step_);
    pending_step_.reset();
  } else if (mechanisms_.empty()) {
    if (transport_availability_.transport_list_did_include_internal) {
      SetCurrentStep(Step::kErrorNoPasskeys);
    } else {
      SetCurrentStep(Step::kErrorNoAvailableTransports);
    }
  } else if (ephemeral_state_.priority_mechanism_index_) {
    Mechanism& mechanism =
        mechanisms_[*ephemeral_state_.priority_mechanism_index_];
    const Mechanism::Credential* cred =
        absl::get_if<Mechanism::Credential>(&mechanism.type);
    if (cred != nullptr &&
        // Credentials on phones should never be triggered automatically.
        (cred->value().source == device::AuthenticatorType::kPhone ||
         (transport_availability_.has_empty_allow_list &&
          // iCloud Keychain has its own confirmation UI and we don't want to
          // duplicate it.
          cred->value().source !=
              device::AuthenticatorType::kICloudKeychain))) {
      SetCurrentStep(Step::kSelectPriorityMechanism);
    } else {
      mechanism.callback.Run();
    }
  } else {
    // If an allowlist was included and there are matches on a local
    // authenticator, jump to it. There are no mechanisms for these
    // authenticators so `priority_mechanism_index_` cannot handle this.
    if (base::FeatureList::IsEnabled(device::kWebAuthnNewPasskeyUI) &&
        !transport_availability_.has_empty_allow_list) {
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
      if (transport_availability_.has_platform_authenticator_credential ==
          device::FidoRequestHandlerBase::RecognizedCredential::
              kNoRecognizedCredential) {
        // If there are no local matches but there are phone passkeys, jump to
        // the phone from sync.
        for (auto& mechanism : mechanisms_) {
          const auto& type = mechanism.type;
          if (absl::holds_alternative<Mechanism::Credential>(type) &&
              absl::get<Mechanism::Credential>(type)->source ==
                  device::AuthenticatorType::kPhone) {
            SetCurrentStep(Step::kPhoneConfirmationSheet);
            return;
          }
        }
      }
    }
    // If a request only includes mechanisms that can be serviced by the Windows
    // API and local credentials, there is no point showing Chrome UI as an
    // extra step. Jump to Windows instead.
    if (base::FeatureList::IsEnabled(device::kWebAuthnNewPasskeyUI) &&
        transport_availability_.has_win_native_api_authenticator &&
        base::ranges::all_of(mechanisms_, [](const auto& mech) {
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
    SetCurrentStep(Step::kMechanismSelection);
  }
}

void AuthenticatorRequestDialogModel::OnPhoneContactFailed(
    const std::string& name) {
  ContactNextPhoneByName(name);
}

void AuthenticatorRequestDialogModel::OnCableEvent(
    device::cablev2::Event event) {
  switch (event) {
    case device::cablev2::Event::kPhoneConnected:
    case device::cablev2::Event::kBLEAdvertReceived:
      if (current_step_ != Step::kCableV2Connecting) {
        SetCurrentStep(Step::kCableV2Connecting);
        cable_connecting_sheet_timer_.Start(
            FROM_HERE, base::Milliseconds(1250),
            base::BindOnce(&AuthenticatorRequestDialogModel::
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

void AuthenticatorRequestDialogModel::OnCableConnectingTimerComplete() {
  if (cable_connecting_ready_to_advance_ &&
      current_step_ == Step::kCableV2Connecting) {
    SetCurrentStep(Step::kCableV2Connected);
  }
}

void AuthenticatorRequestDialogModel::StartPhonePairing() {
  DCHECK(cable_qr_string_);
  SetCurrentStep(Step::kCableV2QRCode);
}

void AuthenticatorRequestDialogModel::
    EnsureBleAdapterIsPoweredAndContinueWithStep(Step step) {
  DCHECK(current_step() == Step::kMechanismSelection ||
         current_step() == Step::kUsbInsertAndActivate ||
         current_step() == Step::kCableActivate ||
         current_step() == Step::kAndroidAccessory ||
         current_step() == Step::kOffTheRecordInterstitial ||
         current_step() == Step::kPreSelectAccount ||
         current_step() == Step::kSelectPriorityMechanism ||
         current_step() == Step::kSelectAccount ||
         current_step() == Step::kConditionalMediation ||
         current_step() == Step::kNotStarted)
      << "Invalid step " << static_cast<int>(current_step());

#if BUILDFLAG(IS_MAC)
  if (transport_availability()->ble_access_denied) {
    // |step| is not saved because macOS asks the user to restart Chrome
    // after permission has been granted. So the user will end up retrying
    // the whole WebAuthn request in the new process.
    SetCurrentStep(Step::kBlePermissionMac);
    return;
  }
#endif

  if (ble_adapter_is_powered()) {
    SetCurrentStep(step);
    return;
  }

  after_ble_adapter_powered_ =
      base::BindOnce(&AuthenticatorRequestDialogModel::SetCurrentStep,
                     weak_factory_.GetWeakPtr(), step);

  if (transport_availability()->can_power_on_ble_adapter) {
    SetCurrentStep(Step::kBlePowerOnAutomatic);
  } else {
    SetCurrentStep(Step::kBlePowerOnManual);
  }
}

void AuthenticatorRequestDialogModel::ContinueWithFlowAfterBleAdapterPowered() {
  DCHECK(current_step() == Step::kBlePowerOnManual ||
         current_step() == Step::kBlePowerOnAutomatic);
  DCHECK(ble_adapter_is_powered());

  std::move(after_ble_adapter_powered_).Run();
}

void AuthenticatorRequestDialogModel::PowerOnBleAdapter() {
  DCHECK_EQ(current_step(), Step::kBlePowerOnAutomatic);
  if (!bluetooth_adapter_power_on_callback_) {
    return;
  }

  bluetooth_adapter_power_on_callback_.Run();
}

#if BUILDFLAG(IS_MAC)
void AuthenticatorRequestDialogModel::OpenBlePreferences() {
  DCHECK_EQ(current_step(), Step::kBlePermissionMac);
  base::mac::OpenSystemSettingsPane(
      base::mac::SystemSettingsPane::kPrivacySecurity_Bluetooth);
}
#endif  // IS_MAC

void AuthenticatorRequestDialogModel::TryUsbDevice() {
  DCHECK_EQ(current_step(), Step::kUsbInsertAndActivate);
}

void AuthenticatorRequestDialogModel::StartPlatformAuthenticatorFlow() {
  // Never try the platform authenticator if the request is known in advance to
  // fail. Proceed to a special error screen instead.
  if (transport_availability_.request_type ==
      device::FidoRequestType::kGetAssertion) {
    DCHECK_NE(transport_availability_.has_platform_authenticator_credential,
              device::FidoRequestHandlerBase::RecognizedCredential::kUnknown);
    if (transport_availability_.has_platform_authenticator_credential ==
        device::FidoRequestHandlerBase::RecognizedCredential::
            kNoRecognizedCredential) {
      SetCurrentStep(Step::kErrorInternalUnrecognized);
      return;
    }

    // If the platform authenticator reports known credentials, show them in the
    // UI.
    if (!transport_availability_.recognized_credentials.empty()) {
      if (transport_availability_.has_empty_allow_list) {
        // For discoverable credential requests, show an account picker.
        if (base::FeatureList::IsEnabled(device::kWebAuthnNewPasskeyUI)) {
          ephemeral_state_.creds_ =
              RecognizedCredentialsFor(device::AuthenticatorType::kTouchID);
        } else {
          ephemeral_state_.creds_ =
              transport_availability_.recognized_credentials;
        }
        SetCurrentStep(ephemeral_state_.creds_.size() == 1
                           ? Step::kPreSelectSingleAccount
                           : Step::kPreSelectAccount);
      } else {
        // For requests with an allow list, pre-select a random credential. On
        // macOS, iCloud Keychain is started via a different function, so don't
        // select an iCloud Keychain credential.
        if (base::FeatureList::IsEnabled(device::kWebAuthnNewPasskeyUI)) {
          const auto cred =
              RecognizedCredentialsFor(device::AuthenticatorType::kTouchID)
                  .at(0);
          ephemeral_state_.creds_ = {std::move(cred)};
        } else {
          ephemeral_state_.creds_ = {
              transport_availability_.recognized_credentials.front()};
        }
#if BUILDFLAG(IS_MAC)
        if (base::FeatureList::IsEnabled(
                device::kWebAuthnSkipSingleAccountMacOS) &&
            (transport_availability_.user_verification_requirement ==
                 device::UserVerificationRequirement::kRequired ||
             local_biometrics_override_for_testing_.value_or(
                 device::fido::mac::DeviceHasBiometricsAvailable()))) {
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
          base::BindOnce(&AuthenticatorRequestDialogModel::
                             HideDialogAndDispatchToPlatformAuthenticator,
                         weak_factory_.GetWeakPtr(), absl::nullopt);
      SetCurrentStep(Step::kOffTheRecordInterstitial);
      return;
    }
  }

  HideDialogAndDispatchToPlatformAuthenticator();
}

void AuthenticatorRequestDialogModel::OnOffTheRecordInterstitialAccepted() {
  std::move(after_off_the_record_interstitial_).Run();
}

void AuthenticatorRequestDialogModel::ShowCableUsbFallback() {
  DCHECK_EQ(current_step(), Step::kCableActivate);
  SetCurrentStep(Step::kAndroidAccessory);
}

void AuthenticatorRequestDialogModel::ShowCable() {
  DCHECK_EQ(current_step(), Step::kAndroidAccessory);
  SetCurrentStep(Step::kCableActivate);
}

void AuthenticatorRequestDialogModel::Cancel() {
  if (use_conditional_mediation_) {
    // Conditional UI requests are never cancelled, they restart silently.
    ResetEphemeralState();
    for (auto& observer : observers_) {
      observer.OnStartOver();
    }
    StartConditionalMediationRequest();
    return;
  }

  if (is_request_complete()) {
    SetCurrentStep(Step::kClosed);
  }

  for (auto& observer : observers_) {
    observer.OnCancelRequest();
  }
}

void AuthenticatorRequestDialogModel::ManageDevices() {
  for (auto& observer : observers_) {
    observer.OnManageDevicesClicked();
  }
}

void AuthenticatorRequestDialogModel::OnSheetModelDidChange() {
  for (auto& observer : observers_) {
    observer.OnSheetModelChanged();
  }
}

void AuthenticatorRequestDialogModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AuthenticatorRequestDialogModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AuthenticatorRequestDialogModel::OnRequestComplete() {
  if (use_conditional_mediation_) {
    auto* render_frame_host = content::RenderFrameHost::FromID(frame_host_id_);
    auto* web_contents = GetWebContents();
    if (web_contents && render_frame_host) {
      ChromeWebAuthnCredentialsDelegateFactory::GetFactory(web_contents)
          ->GetDelegateForFrame(render_frame_host)
          ->NotifyWebAuthnRequestAborted();
    }
  }
  SetCurrentStep(Step::kClosed);
}

void AuthenticatorRequestDialogModel::OnRequestTimeout() {
  // The request may time out while the UI shows a different error.
  if (!is_request_complete()) {
    SetCurrentStep(Step::kTimedOut);
  }
}

void AuthenticatorRequestDialogModel::OnActivatedKeyNotRegistered() {
  DCHECK(!is_request_complete());
  SetCurrentStep(Step::kKeyNotRegistered);
}

void AuthenticatorRequestDialogModel::OnActivatedKeyAlreadyRegistered() {
  DCHECK(!is_request_complete());
  SetCurrentStep(Step::kKeyAlreadyRegistered);
}

void AuthenticatorRequestDialogModel::OnSoftPINBlock() {
  SetCurrentStep(Step::kClientPinErrorSoftBlock);
}

void AuthenticatorRequestDialogModel::OnHardPINBlock() {
  SetCurrentStep(Step::kClientPinErrorHardBlock);
}

void AuthenticatorRequestDialogModel::OnAuthenticatorRemovedDuringPINEntry() {
  SetCurrentStep(Step::kClientPinErrorAuthenticatorRemoved);
}

void AuthenticatorRequestDialogModel::OnAuthenticatorMissingResidentKeys() {
  SetCurrentStep(Step::kMissingCapability);
}

void AuthenticatorRequestDialogModel::OnAuthenticatorMissingUserVerification() {
  SetCurrentStep(Step::kMissingCapability);
}

void AuthenticatorRequestDialogModel::OnAuthenticatorMissingLargeBlob() {
  // TODO(nsatragno): on Windows we should have a more accurate message if large
  // blob is missing.
  SetCurrentStep(Step::kMissingCapability);
}

void AuthenticatorRequestDialogModel::OnNoCommonAlgorithms() {
  SetCurrentStep(Step::kMissingCapability);
}

void AuthenticatorRequestDialogModel::OnAuthenticatorStorageFull() {
  SetCurrentStep(Step::kStorageFull);
}

void AuthenticatorRequestDialogModel::OnUserConsentDenied() {
  if (use_conditional_mediation_) {
    // Do not show a page-modal retry error sheet if the user cancelled out of
    // their platform authenticator during a conditional UI request.
    // Instead, retry silently.
    Cancel();
    return;
  }

  if (ephemeral_state_.did_dispatch_to_icloud_keychain_) {
    // If we dispatched automatically to iCloud Keychain and the
    // user clicked cancel, give them the option to try something else.
    bool did_trigger_automatically =
        ephemeral_state_.did_invoke_platform_despite_no_priority_mechanism_;
    if (!did_trigger_automatically &&
        ephemeral_state_.priority_mechanism_index_.has_value()) {
      const auto& priority_type =
          mechanisms_[*ephemeral_state_.priority_mechanism_index_].type;
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
      Cancel();
    }
    return;
  }

  SetCurrentStep(Step::kErrorInternalUnrecognized);
}

bool AuthenticatorRequestDialogModel::OnWinUserCancelled() {
#if BUILDFLAG(IS_WIN)
  if (use_conditional_mediation_) {
    // Do not show a page-modal retry error sheet if the user cancelled out of
    // their platform authenticator during a conditional UI request.
    // Instead, retry silently.
    Cancel();
    return true;
  }

  // If the native Windows API was triggered immediately (i.e. before any Chrome
  // dialog) then start the request over (once) if the user cancels the Windows
  // UI and there are other options in Chrome's UI. But if Windows supports
  // hybrid then we've nothing more to offer in practice.
  if (!WebAuthnApiSupportsHybrid()) {
    bool have_other_option =
        base::ranges::any_of(mechanisms_, [](const Mechanism& m) -> bool {
          return absl::holds_alternative<Mechanism::Phone>(m.type) ||
                 absl::holds_alternative<Mechanism::AddPhone>(m.type);
        });
    bool windows_was_priority =
        ephemeral_state_.did_invoke_platform_despite_no_priority_mechanism_ ||
        (ephemeral_state_.priority_mechanism_index_ &&
         absl::holds_alternative<Mechanism::WindowsAPI>(
             mechanisms_[*ephemeral_state_.priority_mechanism_index_].type));
    if (have_other_option && windows_was_priority) {
      StartOver();
      return true;
    }
  }
#endif

  return false;
}

bool AuthenticatorRequestDialogModel::OnHybridTransportError() {
  SetCurrentStep(Step::kCableV2Error);
  return true;
}

bool AuthenticatorRequestDialogModel::OnNoPasskeys() {
  SetCurrentStep(Step::kErrorNoPasskeys);
  return true;
}

void AuthenticatorRequestDialogModel::OnBluetoothPoweredStateChanged(
    bool powered) {
  transport_availability_.is_ble_powered = powered;

  for (auto& observer : observers_) {
    observer.OnBluetoothPoweredStateChanged();
  }

  // For the manual flow, the user has to click the "next" button explicitly.
  if (current_step() == Step::kBlePowerOnAutomatic) {
    ContinueWithFlowAfterBleAdapterPowered();
  }
}

void AuthenticatorRequestDialogModel::SetRequestCallback(
    RequestCallback request_callback) {
  request_callback_ = request_callback;
}

void AuthenticatorRequestDialogModel::SetAccountPreselectedCallback(
    content::AuthenticatorRequestClientDelegate::AccountPreselectedCallback
        callback) {
  account_preselected_callback_ = callback;
}

void AuthenticatorRequestDialogModel::SetBluetoothAdapterPowerOnCallback(
    base::RepeatingClosure bluetooth_adapter_power_on_callback) {
  bluetooth_adapter_power_on_callback_ = bluetooth_adapter_power_on_callback;
}

void AuthenticatorRequestDialogModel::OnHavePIN(std::u16string pin) {
  if (!pin_callback_) {
    // Protect against the view submitting a PIN more than once without
    // receiving a matching response first. |CollectPIN| is called again if
    // the user needs to be prompted for a retry.
    return;
  }
  std::move(pin_callback_).Run(pin);
}

void AuthenticatorRequestDialogModel::OnRetryUserVerification(int attempts) {
  uv_attempts_ = attempts;
  SetCurrentStep(Step::kRetryInternalUserVerification);
}

void AuthenticatorRequestDialogModel::OnResidentCredentialConfirmed() {
  DCHECK_EQ(current_step(), Step::kResidentCredentialConfirmation);
  HideDialogAndDispatchToPlatformAuthenticator();
}

void AuthenticatorRequestDialogModel::OnAttestationPermissionResponse(
    bool attestation_permission_granted) {
  if (!attestation_callback_) {
    return;
  }
  std::move(attestation_callback_).Run(attestation_permission_granted);
}

void AuthenticatorRequestDialogModel::AddAuthenticator(
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

void AuthenticatorRequestDialogModel::RemoveAuthenticator(
    base::StringPiece authenticator_id) {
  ephemeral_state_.saved_authenticators_.RemoveAuthenticator(authenticator_id);
}

// SelectAccount is called to trigger an account selection dialog.
void AuthenticatorRequestDialogModel::SelectAccount(
    std::vector<device::AuthenticatorGetAssertionResponse> responses,
    base::OnceCallback<void(device::AuthenticatorGetAssertionResponse)>
        callback) {
  ephemeral_state_.responses_ = std::move(responses);
  ephemeral_state_.creds_ = {};
  for (const auto& response : ephemeral_state_.responses_) {
    ephemeral_state_.creds_.emplace_back(
        device::AuthenticatorType::kOther, relying_party_id_,
        response.credential->id, *response.user_entity);
  }
  selection_callback_ = std::move(callback);
  SetCurrentStep(ephemeral_state_.creds_.size() == 1
                     ? Step::kSelectSingleAccount
                     : Step::kSelectAccount);
}

void AuthenticatorRequestDialogModel::OnAccountSelected(size_t index) {
  if (!selection_callback_) {
    // It's possible that the user could activate the dialog more than once
    // before the Webauthn request is completed and its torn down.
    return;
  }

  device::AuthenticatorGetAssertionResponse response =
      std::move(ephemeral_state_.responses_.at(index));
  ephemeral_state_.creds_.clear();
  ephemeral_state_.responses_.clear();
  std::move(selection_callback_).Run(std::move(response));
}

void AuthenticatorRequestDialogModel::OnAccountPreselected(
    const std::vector<uint8_t>& credential_id) {
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
  account_preselected_callback_.Run(device::PublicKeyCredentialDescriptor(
      device::CredentialType::kPublicKey, cred->cred_id,
      {cred->source == device::AuthenticatorType::kPhone
           ? AuthenticatorTransport::kHybrid
           : AuthenticatorTransport::kInternal}));
  ephemeral_state_.creds_.clear();
  if (source == device::AuthenticatorType::kPhone) {
    ContactPriorityPhone();
  } else {
    HideDialogAndDispatchToPlatformAuthenticator(source);
  }
}

void AuthenticatorRequestDialogModel::OnAccountPreselectedIndex(size_t index) {
  OnAccountPreselected(ephemeral_state_.creds_.at(index).cred_id);
}

void AuthenticatorRequestDialogModel::SetSelectedAuthenticatorForTesting(
    AuthenticatorReference test_authenticator) {
  ephemeral_state_.selected_authenticator_id_ =
      test_authenticator.authenticator_id;
  ephemeral_state_.saved_authenticators_.AddAuthenticator(
      std::move(test_authenticator));
}

base::span<const AuthenticatorRequestDialogModel::Mechanism>
AuthenticatorRequestDialogModel::mechanisms() const {
  return mechanisms_;
}

void AuthenticatorRequestDialogModel::ContactPriorityPhone() {
  ContactPhone(paired_phones_[*priority_phone_index_]->name);
}

void AuthenticatorRequestDialogModel::ContactPhoneForTesting(
    const std::string& name) {
  // Ensure BLE is powered so that `ContactPhone()` shows the "Check your phone"
  // screen right away.
  transport_availability_.is_ble_powered = true;
  ContactPhone(name);
}

absl::optional<std::u16string>
AuthenticatorRequestDialogModel::GetPriorityPhoneName() const {
  if (!priority_phone_index_) {
    return absl::nullopt;
  }
  return base::UTF8ToUTF16(paired_phones_[*priority_phone_index_]->name);
}

void AuthenticatorRequestDialogModel::StartTransportFlowForTesting(
    AuthenticatorTransport transport) {
  StartGuidedFlowForTransport(transport);
}

void AuthenticatorRequestDialogModel::SetCurrentStepForTesting(Step step) {
  SetCurrentStep(step);
}

bool AuthenticatorRequestDialogModel::cable_should_suggest_usb() const {
  // Offer AoA only for linked caBLEv2 authenticators, not caBLEv1.
  return cable_ui_type_ != CableUIType::CABLE_V1 &&
         base::Contains(transport_availability_.available_transports,
                        AuthenticatorTransport::kAndroidAccessory);
}

void AuthenticatorRequestDialogModel::CollectPIN(
    device::pin::PINEntryReason reason,
    device::pin::PINEntryError error,
    uint32_t min_pin_length,
    int attempts,
    base::OnceCallback<void(std::u16string)> provide_pin_cb) {
  pin_callback_ = std::move(provide_pin_cb);
  min_pin_length_ = min_pin_length;
  pin_error_ = error;
  switch (reason) {
    case device::pin::PINEntryReason::kChallenge:
      pin_attempts_ = attempts;
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

void AuthenticatorRequestDialogModel::FinishCollectToken() {
  SetCurrentStep(Step::kClientPinTapAgain);
}

void AuthenticatorRequestDialogModel::StartInlineBioEnrollment(
    base::OnceClosure next_callback) {
  max_bio_samples_ = absl::nullopt;
  bio_samples_remaining_ = absl::nullopt;
  bio_enrollment_callback_ = std::move(next_callback);
  SetCurrentStep(Step::kInlineBioEnrollment);
}

void AuthenticatorRequestDialogModel::OnSampleCollected(
    int bio_samples_remaining) {
  DCHECK(current_step_ == Step::kInlineBioEnrollment);

  bio_samples_remaining_ = bio_samples_remaining;
  if (!max_bio_samples_) {
    max_bio_samples_ = bio_samples_remaining + 1;
  }
  OnSheetModelDidChange();
}

void AuthenticatorRequestDialogModel::OnBioEnrollmentDone() {
  std::move(bio_enrollment_callback_).Run();
}

void AuthenticatorRequestDialogModel::RequestAttestationPermission(
    bool is_enterprise_attestation,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(current_step_ != Step::kClosed);
  attestation_callback_ = std::move(callback);
  SetCurrentStep(is_enterprise_attestation
                     ? Step::kEnterpriseAttestationPermissionRequest
                     : Step::kAttestationPermissionRequest);
}

void AuthenticatorRequestDialogModel::set_cable_transport_info(
    absl::optional<bool> extension_is_v2,
    std::vector<std::unique_ptr<device::cablev2::Pairing>> paired_phones,
    base::RepeatingCallback<void(std::unique_ptr<device::cablev2::Pairing>)>
        contact_phone_callback,
    const absl::optional<std::string>& cable_qr_string) {
  DCHECK(paired_phones.empty() || contact_phone_callback);

  if (extension_is_v2.has_value()) {
    cable_extension_provided_ = true;
    if (*extension_is_v2) {
      cable_ui_type_ = CableUIType::CABLE_V2_SERVER_LINK;
    } else {
      cable_ui_type_ = CableUIType::CABLE_V1;
    }
  } else {
    cable_ui_type_ = CableUIType::CABLE_V2_2ND_FACTOR;
  }

  paired_phones_ = std::move(paired_phones);
  contact_phone_callback_ = std::move(contact_phone_callback);
  cable_qr_string_ = cable_qr_string;

  paired_phones_contacted_.assign(paired_phones_.size(), false);
}

std::vector<std::string> AuthenticatorRequestDialogModel::paired_phone_names()
    const {
  std::vector<std::string> names;
  base::ranges::transform(paired_phones_, std::back_inserter(names),
                          &device::cablev2::Pairing::name);
  names.erase(std::unique(names.begin(), names.end()), names.end());
  return names;
}

void AuthenticatorRequestDialogModel::set_allow_icloud_keychain(
    bool is_allowed) {
  allow_icloud_keychain_ = is_allowed;
}

void AuthenticatorRequestDialogModel::set_should_create_in_icloud_keychain(
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

void AuthenticatorRequestDialogModel::RecordMacOsStartedHistogram() {
  if (is_non_webauthn_request_ || relying_party_id_ == "google.com") {
    return;
  }

  absl::optional<MacOsHistogramValues> v;
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

void AuthenticatorRequestDialogModel::RecordMacOsSuccessHistogram(
    device::FidoRequestType request_type,
    device::AuthenticatorType authenticator_type) {
  if (!did_record_macos_start_histogram_) {
    return;
  }

  absl::optional<MacOsHistogramValues> v;

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

void AuthenticatorRequestDialogModel::set_is_active_profile_authenticator_user(
    bool is_active) {
  is_active_profile_authenticator_user_ = is_active;
}

void AuthenticatorRequestDialogModel::set_has_icloud_drive_enabled(
    bool is_enabled) {
  has_icloud_drive_enabled_ = is_enabled;
}

void AuthenticatorRequestDialogModel::set_local_biometrics_override_for_testing(
    bool is_enabled) {
  local_biometrics_override_for_testing_ = is_enabled;
}

#endif

base::WeakPtr<AuthenticatorRequestDialogModel>
AuthenticatorRequestDialogModel::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

content::WebContents* AuthenticatorRequestDialogModel::GetWebContents() {
  return content::WebContents::FromRenderFrameHost(
      content::RenderFrameHost::FromID(frame_host_id_));
}

void AuthenticatorRequestDialogModel::SetCurrentStep(Step step) {
  if (!started_) {
    // Dialog isn't showing yet. Remember to show this step when it appears.
    pending_step_ = step;
    return;
  }

  current_step_ = step;

  // Reset state related to automatically advancing the state.
  cable_connecting_sheet_timer_.Stop();
  cable_connecting_ready_to_advance_ = false;

  if (should_dialog_be_closed()) {
    // The dialog will close itself.
    showing_dialog_ = false;
  } else {
    auto* web_contents = GetWebContents();
    if (!showing_dialog_ && web_contents) {
      ShowAuthenticatorRequestDialog(web_contents, this);
      showing_dialog_ = true;
    }
  }

  for (auto& observer : observers_) {
    observer.OnStepTransition();
  }
}

void AuthenticatorRequestDialogModel::StartGuidedFlowForTransport(
    AuthenticatorTransport transport) {
  DCHECK(current_step() == Step::kMechanismSelection ||
         current_step() == Step::kUsbInsertAndActivate ||
         current_step() == Step::kCableActivate ||
         current_step() == Step::kAndroidAccessory ||
         current_step() == Step::kConditionalMediation ||
         current_step() == Step::kCreatePasskey ||
         current_step() == Step::kPreSelectAccount ||
         current_step() == Step::kSelectPriorityMechanism ||
         current_step() == Step::kSelectAccount ||
         current_step() == Step::kNotStarted);
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

void AuthenticatorRequestDialogModel::StartGuidedFlowForAddPhone() {
  EnsureBleAdapterIsPoweredAndContinueWithStep(Step::kCableV2QRCode);
}

void AuthenticatorRequestDialogModel::StartWinNativeApi() {
  DCHECK(transport_availability_.has_win_native_api_authenticator);
  if (transport_availability_.request_is_internal_only &&
      !transport_availability_.win_is_uvpaa) {
    offer_try_again_in_ui_ = false;
    SetCurrentStep(Step::kErrorWindowsHelloNotEnabled);
    return;
  }

  if (resident_key_requirement() !=
          device::ResidentKeyRequirement::kDiscouraged &&
      !transport_availability_.win_native_ui_shows_resident_credential_notice) {
    SetCurrentStep(Step::kResidentCredentialConfirmation);
  } else {
    HideDialogAndDispatchToPlatformAuthenticator();
  }
}

void AuthenticatorRequestDialogModel::StartICloudKeychain() {
  DCHECK(transport_availability_.has_icloud_keychain);
  if (transport_availability_.has_icloud_keychain_credential ==
          device::FidoRequestHandlerBase::RecognizedCredential::
              kHasRecognizedCredential &&
      !transport_availability_.has_empty_allow_list) {
    // For requests with an allow list, pre-select a random credential.
    const auto cred =
        RecognizedCredentialsFor(device::AuthenticatorType::kICloudKeychain)[0];
    account_preselected_callback_.Run(device::PublicKeyCredentialDescriptor(
        device::CredentialType::kPublicKey, cred.cred_id,
        {AuthenticatorTransport::kInternal}));
  }

  HideDialogAndDispatchToPlatformAuthenticator(
      device::AuthenticatorType::kICloudKeychain);
}

void AuthenticatorRequestDialogModel::ContactPhone(const std::string& name) {
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
        base::BindOnce(&AuthenticatorRequestDialogModel::
                           ContactPhoneAfterOffTheRecordInterstitial,
                       weak_factory_.GetWeakPtr(), name);
    SetCurrentStep(Step::kOffTheRecordInterstitial);
    return;
  }

  ContactPhoneAfterOffTheRecordInterstitial(name);
}

void AuthenticatorRequestDialogModel::ContactPhoneAfterOffTheRecordInterstitial(
    std::string name) {
  if (!ble_adapter_is_powered()) {
    after_ble_adapter_powered_ = base::BindOnce(
        &AuthenticatorRequestDialogModel::ContactPhoneAfterBleIsPowered,
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

void AuthenticatorRequestDialogModel::ContactPhoneAfterBleIsPowered(
    std::string name) {
  ContactNextPhoneByName(name);
  SetCurrentStep(Step::kCableActivate);
}

void AuthenticatorRequestDialogModel::StartConditionalMediationRequest() {
  ephemeral_state_.creds_ = transport_availability_.recognized_credentials;

  auto* render_frame_host = content::RenderFrameHost::FromID(frame_host_id_);
  auto* web_contents = GetWebContents();
  if (web_contents && render_frame_host) {
    std::vector<password_manager::PasskeyCredential> credentials;
    absl::optional<size_t> priority_phone_index =
        GetIndexOfMostRecentlyUsedPhoneFromSync();
    absl::optional<std::u16string> priority_phone_name;
    if (priority_phone_index) {
      priority_phone_name =
          base::UTF8ToUTF16(paired_phones_[*priority_phone_index]->name);
    }
    for (const auto& credential : ephemeral_state_.creds_) {
      if (credential.source == device::AuthenticatorType::kPhone &&
          !priority_phone_index) {
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

void AuthenticatorRequestDialogModel::DispatchRequestAsync(
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

void AuthenticatorRequestDialogModel::ContactNextPhoneByName(
    const std::string& name) {
  bool found_name = false;
  ephemeral_state_.selected_phone_name_.reset();
  for (size_t i = 0; i != paired_phones_.size(); i++) {
    const std::unique_ptr<device::cablev2::Pairing>& phone = paired_phones_[i];
    if (phone->name == name) {
      found_name = true;
      ephemeral_state_.selected_phone_name_ = name;
      if (!paired_phones_contacted_[i]) {
        MaybeStoreLastUsedPairing(
            content::RenderFrameHost::FromID(frame_host_id_),
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

absl::optional<size_t>
AuthenticatorRequestDialogModel::GetIndexOfMostRecentlyUsedPhoneFromSync()
    const {
  // Try finding the most recently used phone from sync.
  absl::optional<std::vector<uint8_t>> last_used_pairing =
      RetrieveLastUsedPairing(content::RenderFrameHost::FromID(frame_host_id_));
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
  absl::optional<int> ret;
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

void AuthenticatorRequestDialogModel::SortRecognizedCredentials() {
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

void AuthenticatorRequestDialogModel::PopulateMechanisms() {
  const bool is_get_assertion = transport_availability_.request_type ==
                                device::FidoRequestType::kGetAssertion;
  const bool is_new_get_assertion_ui =
      is_get_assertion &&
      base::FeatureList::IsEnabled(device::kWebAuthnNewPasskeyUI);
  priority_phone_index_ = GetIndexOfMostRecentlyUsedPhoneFromSync();
  absl::optional<std::u16string> priority_phone_name = GetPriorityPhoneName();
  bool list_phone_passkeys =
      is_new_get_assertion_ui && priority_phone_index_ &&
      base::FeatureList::IsEnabled(device::kWebAuthnListSyncedPasskeys);
  bool specific_phones_listed = false;
  bool specific_local_passkeys_listed = false;
  if (is_new_get_assertion_ui && !use_conditional_mediation_) {
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
      if (cred.source == device::AuthenticatorType::kPhone) {
        specific_phones_listed = true;
      } else {
        specific_local_passkeys_listed = true;
      }
      std::u16string name = base::UTF8ToUTF16(cred.user.name.value_or(""));
      auto& mechanism = mechanisms_.emplace_back(
          AuthenticatorRequestDialogModel::Mechanism::Credential(
              {cred.source, cred.user.id}),
          name, name, GetCredentialIcon(cred.source),
          base::BindRepeating(
              &AuthenticatorRequestDialogModel::OnAccountPreselected,
              base::Unretained(this), cred.cred_id));
      mechanism.description =
          GetMechanismDescription(cred.source, priority_phone_name);
    }
  }

  std::vector<AuthenticatorTransport> transports_to_list_if_active;
  // Do not list the internal transport if we can offer users to select a
  // platform credential directly. This is true for both conditional requests
  // and the new passkey selector UI.
  bool did_enumerate_local_passkeys = false;
  if (use_conditional_mediation_) {
    did_enumerate_local_passkeys = true;
  } else if (is_new_get_assertion_ui) {
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
  if (!base::FeatureList::IsEnabled(device::kWebAuthnNewPasskeyUI)) {
    transports_to_list_if_active.push_back(
        AuthenticatorTransport::kUsbHumanInterfaceDevice);
  }

  const auto kCable = AuthenticatorTransport::kHybrid;
  const bool windows_handles_hybrid = WebAuthnApiSupportsHybrid();
  bool include_add_phone_option = false;

  if (cable_ui_type_) {
    switch (*cable_ui_type_) {
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
          offer_try_again_in_ui_ = false;
        }
        break;
      }
    }
  }

  if (transport_availability_.has_icloud_keychain && allow_icloud_keychain_ &&
      // The mechanism for iCloud Keychain only appears for create(), or if
      // Chrome doesn't have permission to enumerate credentials and thus the
      // user needs a generic mechanism to trigger it.
      (!is_new_get_assertion_ui ||
       transport_availability_.has_icloud_keychain_credential ==
           device::FidoRequestHandlerBase::RecognizedCredential::kUnknown)) {
    const std::u16string name =
        l10n_util::GetStringUTF16(IDS_WEBAUTHN_TRANSPORT_ICLOUD_KEYCHAIN);
    mechanisms_.emplace_back(
        Mechanism::ICloudKeychain(), name, name, kIcloudKeychainIcon,
        base::BindRepeating(
            &AuthenticatorRequestDialogModel::StartICloudKeychain,
            base::Unretained(this)));
  }

  absl::optional<std::pair<int, AuthenticatorTransport>> windows_button_label;
  if (base::FeatureList::IsEnabled(device::kWebAuthnNewPasskeyUI)) {
    windows_button_label = GetWindowsAPIButtonLabel(transport_availability_,
                                                    specific_phones_listed);
  } else if (win_native_api_enabled()) {
    windows_button_label =
        std::make_pair(IDS_WEBAUTHN_TRANSPORT_WINDOWS_HELLO_OR_SECURITY_KEY,
                       AuthenticatorTransport::kInternal);
  }
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
    for (const auto& phone_name : paired_phone_names()) {
      const std::u16string name16 = base::UTF8ToUTF16(phone_name);
      static constexpr size_t kMaxLongNameChars = 50;
      static constexpr size_t kMaxShortNameChars = 30;
      std::u16string long_name, short_name;
      gfx::ElideString(name16, kMaxLongNameChars, &long_name);
      gfx::ElideString(name16, kMaxShortNameChars, &short_name);

      mechanisms_.emplace_back(
          Mechanism::Phone(phone_name), std::move(long_name),
          std::move(short_name), kSmartphoneIcon,
          base::BindRepeating(&AuthenticatorRequestDialogModel::ContactPhone,
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
      priority_phone_index_ = 0;
      pending_step_ = Step::kPhoneConfirmationSheet;
    }
  }

  if (include_add_phone_option) {
    std::u16string label;
    if (base::FeatureList::IsEnabled(device::kWebAuthnNewPasskeyUI)) {
      label = l10n_util::GetStringUTF16(GetHybridButtonLabel(
          base::Contains(transport_availability_.available_transports,
                         AuthenticatorTransport::kUsbHumanInterfaceDevice),
          specific_phones_listed));
    } else {
      label = l10n_util::GetStringUTF16(
          specific_phones_listed
              ? IDS_WEBAUTHN_PASSKEY_DIFFERENT_PHONE_OR_TABLET_LABEL
              : IDS_WEBAUTHN_PASSKEY_PHONE_OR_TABLET_LABEL);
    }
    mechanisms_.emplace_back(
        Mechanism::AddPhone(), label, label, kQrcodeGeneratorIcon,
        base::BindRepeating(
            &AuthenticatorRequestDialogModel::StartGuidedFlowForAddPhone,
            base::Unretained(this)));
  }
  if (base::FeatureList::IsEnabled(device::kWebAuthnNewPasskeyUI) &&
      (!include_add_phone_option || !transport_availability_.is_ble_powered ||
       transport_availability_.ble_access_denied)) {
    // If the new UI is enabled, only show USB as an option if the QR code is
    // not available or if tapping it would trigger a prompt to enable BLE.
    transports_to_list_if_active.push_back(
        AuthenticatorTransport::kUsbHumanInterfaceDevice);
  }

  for (const auto transport : transports_to_list_if_active) {
    if (!base::Contains(transport_availability_.available_transports,
                        transport)) {
      continue;
    }

    mechanisms_.emplace_back(
        Mechanism::Transport(transport), GetTransportDescription(transport),
        GetTransportShortDescription(transport), GetTransportIcon(transport),
        base::BindRepeating(
            &AuthenticatorRequestDialogModel::StartGuidedFlowForTransport,
            base::Unretained(this), transport));
  }
  // Add the Windows native API button last if it does not do Windows Hello.
  if (windows_button_label &&
      windows_button_label->second != AuthenticatorTransport::kInternal) {
    AddWindowsButton(windows_button_label->first, windows_button_label->second);
  }
}

void AuthenticatorRequestDialogModel::AddWindowsButton(
    int label,
    AuthenticatorTransport transport) {
  const std::u16string desc = l10n_util::GetStringUTF16(label);
  mechanisms_.emplace_back(
      Mechanism::WindowsAPI(), desc, desc, GetTransportIcon(transport),
      base::BindRepeating(&AuthenticatorRequestDialogModel::StartWinNativeApi,
                          base::Unretained(this)));
}

absl::optional<size_t>
AuthenticatorRequestDialogModel::IndexOfPriorityMechanism() {
  if (base::FeatureList::IsEnabled(device::kWebAuthnNewPasskeyUI) &&
      transport_availability_.request_type ==
          device::FidoRequestType::kGetAssertion) {
    // If there is a single mechanism, go to that.
    if (mechanisms_.size() == 1) {
      return 0;
    }

    if (transport_availability_.has_empty_allow_list) {
      // The index and info of the credential that the UI should default to.
      absl::optional<std::pair<size_t, const Mechanism::CredentialInfo*>>
          best_cred;
      bool multiple_distinct_creds = false;

      for (size_t i = 0; i < mechanisms_.size(); ++i) {
        const auto& type = mechanisms_[i].type;
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
      // If there one of the passkey is a valid default, go to that.
      if (!multiple_distinct_creds && best_cred.has_value()) {
        return best_cred->first;
      }
    }

    // If it's caBLEv1, or server-linked caBLEv2, jump to that.
    if (cable_ui_type_) {
      switch (*cable_ui_type_) {
        case AuthenticatorRequestDialogModel::CableUIType::CABLE_V2_SERVER_LINK:
        case AuthenticatorRequestDialogModel::CableUIType::CABLE_V1:
          for (size_t i = 0; i < mechanisms_.size(); ++i) {
            if (mechanisms_[i].type == Mechanism::Type(Mechanism::Transport(
                                           AuthenticatorTransport::kHybrid))) {
              return i;
            }
          }
          break;
        case AuthenticatorRequestDialogModel::CableUIType::CABLE_V2_2ND_FACTOR:
          break;
      }
    }

    // For all other cases, go to the multi source passkey picker.
    return absl::nullopt;
  }
  if (mechanisms_.size() == 1) {
    return 0;
  } else if (mechanisms_.empty()) {
    return absl::nullopt;
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
      if (cable_ui_type_) {
        switch (*cable_ui_type_) {
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

    if (is_passkey_request && paired_phone_names().empty() &&
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

    if (windows_handles_hybrid) {
      // If Windows supports hybrid then we defer to Windows in all cases.
      priority_list.emplace_back(Mechanism::WindowsAPI());
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

    const bool is_passkey_request =
        resident_key_requirement() !=
        device::ResidentKeyRequirement::kDiscouraged;
    if (is_passkey_request) {
      // If attachment=any, then don't jump to suggesting a phone.
      // TODO(crbug.com/1426628): makeCredential requests should always have
      // `make_credential_attachment` set. Stop being hesitant.
      if ((!transport_availability_.make_credential_attachment ||
           *transport_availability_.make_credential_attachment !=
               device::AuthenticatorAttachment::kAny) &&
          paired_phone_names().empty()) {
        priority_list.emplace_back(Mechanism::AddPhone());
      }
    } else {
      // This seems like it might be an error (crbug.com/1426244) as we might
      // still want to jump to platform authenticators for passkey requests if
      // we don't jump to a phone.
      if (kShowCreatePlatformPasskeyStep) {
        priority_list.emplace_back(
            Mechanism::Transport(AuthenticatorTransport::kInternal));
      }
      priority_list.emplace_back(Mechanism::WindowsAPI());
    }
  }

  for (const auto& priority_mechanism : priority_list) {
    // A phone should never be triggered immediately.
    CHECK(!absl::holds_alternative<Mechanism::Phone>(priority_mechanism));

    for (size_t i = 0; i < mechanisms_.size(); i++) {
      if (priority_mechanism == mechanisms_[i].type) {
        return i;
      }
    }
  }

  return absl::nullopt;
}

std::vector<device::DiscoverableCredentialMetadata>
AuthenticatorRequestDialogModel::RecognizedCredentialsFor(
    device::AuthenticatorType source) {
  std::vector<device::DiscoverableCredentialMetadata> ret;
  for (const auto& cred : transport_availability_.recognized_credentials) {
    if (cred.source == source ||
        // kOther is also accepted for unittests.
        cred.source == device::AuthenticatorType::kOther) {
      ret.push_back(cred);
    }
  }
  return ret;
}

void AuthenticatorRequestDialogModel::
    HideDialogAndDispatchToPlatformAuthenticator(
        absl::optional<device::AuthenticatorType> type) {
  HideDialog();

#if BUILDFLAG(IS_WIN)
  // The Windows-native UI already handles retrying so we do not offer a second
  // level of retry in that case.
  offer_try_again_in_ui_ = false;
#elif BUILDFLAG(IS_MAC)
  // If there are multiple platform authenticators, one of them is the default.
  if (!type.has_value() &&
      base::FeatureList::IsEnabled(device::kWebAuthnICloudKeychain)) {
    type = device::AuthenticatorType::kTouchID;
  }
#endif

  auto& authenticators =
      ephemeral_state_.saved_authenticators_.authenticator_list();
  auto platform_authenticator_it = base::ranges::find_if(
      authenticators, [type](const AuthenticatorReference& ref) -> bool {
        return ref.transport == device::FidoTransportProtocol::kInternal &&
               (!type || ref.type == *type ||
                !base::FeatureList::IsEnabled(device::kWebAuthnICloudKeychain));
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
