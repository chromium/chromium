// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/features.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/pin.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_elider.h"

namespace {

// BleEvent enumerates user-visible BLE events.
enum class BleEvent {
  kAlreadyPowered = 0,    // BLE was already powered.
  kNeedsPowerAuto = 1,    // BLE was not powered and so we asked the user.
  kNeedsPowerManual = 2,  // BLE was not powered and so we asked the user, but
                          // they have to do it manually.
  kNewlyPowered = 3,      // BLE wasn't powered, but the user turned it on.

  kMaxValue = kNewlyPowered,
};

constexpr int GetMessageIdForTransportDescription(
    AuthenticatorTransport transport) {
  switch (transport) {
    case AuthenticatorTransport::kUsbHumanInterfaceDevice:
      return IDS_WEBAUTHN_TRANSPORT_USB;
    case AuthenticatorTransport::kInternal:
      return IDS_WEBAUTHN_TRANSPORT_INTERNAL;
    case AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy:
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
    case AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy:
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

constexpr const gfx::VectorIcon* GetTransportIcon(
    AuthenticatorTransport transport) {
  switch (transport) {
    case AuthenticatorTransport::kUsbHumanInterfaceDevice:
      return &vector_icons::kUsbIcon;
    case AuthenticatorTransport::kInternal:
      return &kLaptopIcon;
    case AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy:
      return &kSmartphoneIcon;
    case AuthenticatorTransport::kAndroidAccessory:
      return &kUsbCableIcon;
    case AuthenticatorTransport::kBluetoothLowEnergy:
    case AuthenticatorTransport::kNearFieldCommunication:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace

AuthenticatorRequestDialogModel::EphemeralState::EphemeralState() = default;
AuthenticatorRequestDialogModel::EphemeralState::~EphemeralState() = default;

AuthenticatorRequestDialogModel::Mechanism::Mechanism(
    AuthenticatorRequestDialogModel::Mechanism::Type in_type,
    std::u16string in_name,
    std::u16string in_short_name,
    const gfx::VectorIcon* in_icon,
    base::RepeatingClosure in_callback,
    bool is_priority)
    : type(std::move(in_type)),
      name(std::move(in_name)),
      short_name(std::move(in_short_name)),
      icon(in_icon),
      callback(std::move(in_callback)),
      priority(is_priority) {}
AuthenticatorRequestDialogModel::Mechanism::~Mechanism() = default;
AuthenticatorRequestDialogModel::Mechanism::Mechanism(Mechanism&&) = default;

AuthenticatorRequestDialogModel::PairedPhone::PairedPhone(const PairedPhone&) =
    default;
AuthenticatorRequestDialogModel::PairedPhone::PairedPhone(
    const std::string& name,
    size_t contact_id,
    const std::array<uint8_t, device::kP256X962Length> public_key_x962) {
  this->name = name;
  this->contact_id = contact_id;
  this->public_key_x962 = public_key_x962;
}
AuthenticatorRequestDialogModel::PairedPhone::~PairedPhone() = default;
AuthenticatorRequestDialogModel::PairedPhone&
AuthenticatorRequestDialogModel::PairedPhone::operator=(const PairedPhone&) =
    default;

void AuthenticatorRequestDialogModel::EphemeralState::Reset() {
  selected_authenticator_id_ = absl::nullopt;
  saved_authenticators_.RemoveAllAuthenticators();
  creds_.clear();
}

AuthenticatorRequestDialogModel::AuthenticatorRequestDialogModel(
    const std::string& relying_party_id)
    : relying_party_id_(relying_party_id) {}

AuthenticatorRequestDialogModel::~AuthenticatorRequestDialogModel() {
  for (auto& observer : observers_)
    observer.OnModelDestroyed(this);
}

void AuthenticatorRequestDialogModel::HideDialog() {
  SetCurrentStep(Step::kNotStarted);
}

void AuthenticatorRequestDialogModel::StartFlow(
    TransportAvailabilityInfo transport_availability,
    bool use_location_bar_bubble,
    bool prefer_native_api) {
  DCHECK(!started_);
  DCHECK_EQ(current_step(), Step::kNotStarted);

  started_ = true;
  transport_availability_ = std::move(transport_availability);
  use_location_bar_bubble_ = use_location_bar_bubble;

  PopulateMechanisms(prefer_native_api);

  if (use_location_bar_bubble_) {
    // This is a conditional request so show a lightweight, non-modal dialog
    // instead.
    StartLocationBarBubbleRequest();
  } else {
    StartGuidedFlowForMostLikelyTransportOrShowMechanismSelection();
  }
}

void AuthenticatorRequestDialogModel::StartOver() {
  ephemeral_state_.Reset();

  for (auto& observer : observers_)
    observer.OnStartOver();

  if (use_location_bar_bubble_) {
    StartLocationBarBubbleRequest();
    return;
  }
  current_mechanism_.reset();
  SetCurrentStep(Step::kMechanismSelection);
}

void AuthenticatorRequestDialogModel::
    StartGuidedFlowForMostLikelyTransportOrShowMechanismSelection() {
  DCHECK(current_step() == Step::kNotStarted);

  const auto priority_mechanism_it =
      std::find_if(mechanisms_.begin(), mechanisms_.end(),
                   [](const Mechanism& m) -> bool { return m.priority; });

  if (pending_step_) {
    SetCurrentStep(*pending_step_);
    pending_step_.reset();
  } else if (mechanisms_.empty()) {
    SetCurrentStep(Step::kErrorNoAvailableTransports);
  } else if (mechanisms_.size() == 1) {
    mechanisms_[0].callback.Run();
  } else if (priority_mechanism_it != mechanisms_.end()) {
    priority_mechanism_it->callback.Run();
  } else {
    SetCurrentStep(Step::kMechanismSelection);
  }
}

void AuthenticatorRequestDialogModel::
    HideDialogAndDispatchToNativeWindowsApi() {
  if (!transport_availability()->has_win_native_api_authenticator ||
      transport_availability()->win_native_api_authenticator_id.empty()) {
    NOTREACHED();
    SetCurrentStep(Step::kClosed);
    return;
  }

  // The Windows-native UI already handles retrying so we do not offer a second
  // level of retry in that case.
  offer_try_again_in_ui_ = false;

  // There is no AuthenticatorReference for the Windows authenticator, hence
  // directly call DispatchRequestAsyncInternal here.
  DispatchRequestAsyncInternal(
      transport_availability()->win_native_api_authenticator_id);

  HideDialog();
}

void AuthenticatorRequestDialogModel::OnPhoneContactFailed(
    const std::string& name) {
  ContactNextPhoneByName(name);
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
         current_step() == Step::kNotStarted);

  if (ble_adapter_is_powered()) {
    base::UmaHistogramEnumeration("WebAuthentication.BLEUserEvents",
                                  BleEvent::kAlreadyPowered);
    SetCurrentStep(step);
    return;
  }

  after_ble_adapter_powered_ =
      base::BindOnce(&AuthenticatorRequestDialogModel::SetCurrentStep,
                     weak_factory_.GetWeakPtr(), step);

  BleEvent event;
  if (transport_availability()->can_power_on_ble_adapter) {
    event = BleEvent::kNeedsPowerAuto;
    SetCurrentStep(Step::kBlePowerOnAutomatic);
  } else {
    event = BleEvent::kNeedsPowerManual;
    SetCurrentStep(Step::kBlePowerOnManual);
  }

  base::UmaHistogramEnumeration("WebAuthentication.BLEUserEvents", event);
}

void AuthenticatorRequestDialogModel::ContinueWithFlowAfterBleAdapterPowered() {
  DCHECK(current_step() == Step::kBlePowerOnManual ||
         current_step() == Step::kBlePowerOnAutomatic);
  DCHECK(ble_adapter_is_powered());
  base::UmaHistogramEnumeration("WebAuthentication.BLEUserEvents",
                                BleEvent::kNewlyPowered);

  std::move(after_ble_adapter_powered_).Run();
}

void AuthenticatorRequestDialogModel::PowerOnBleAdapter() {
  DCHECK_EQ(current_step(), Step::kBlePowerOnAutomatic);
  if (!bluetooth_adapter_power_on_callback_)
    return;

  bluetooth_adapter_power_on_callback_.Run();
}

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
  }

  if (transport_availability_.request_type ==
          device::FidoRequestType::kMakeCredential &&
      transport_availability_.is_off_the_record_context) {
    after_off_the_record_interstitial_ =
        base::BindOnce(&AuthenticatorRequestDialogModel::
                           HideDialogAndDispatchToPlatformAuthenticator,
                       weak_factory_.GetWeakPtr());
    SetCurrentStep(Step::kOffTheRecordInterstitial);
    return;
  }

  HideDialogAndDispatchToPlatformAuthenticator();
}

void AuthenticatorRequestDialogModel::OnOffTheRecordInterstitialAccepted() {
  std::move(after_off_the_record_interstitial_).Run();
}

void AuthenticatorRequestDialogModel::ShowCableUsbFallback() {
  DCHECK_EQ(current_step(), Step::kCableActivate);

  switch (experiment_server_link_sheet_) {
    case AuthenticatorRequestDialogModel::ExperimentServerLinkSheet::CONTROL:
    case AuthenticatorRequestDialogModel::ExperimentServerLinkSheet::ARM_2:
    case AuthenticatorRequestDialogModel::ExperimentServerLinkSheet::ARM_5:
    case AuthenticatorRequestDialogModel::ExperimentServerLinkSheet::ARM_6:
      SetCurrentStep(Step::kAndroidAccessory);
      break;
    case AuthenticatorRequestDialogModel::ExperimentServerLinkSheet::ARM_3:
    case AuthenticatorRequestDialogModel::ExperimentServerLinkSheet::ARM_4:
      Cancel();
      break;
  }
}

void AuthenticatorRequestDialogModel::ShowCable() {
  DCHECK_EQ(current_step(), Step::kAndroidAccessory);
  SetCurrentStep(Step::kCableActivate);
}

void AuthenticatorRequestDialogModel::Cancel() {
  if (is_request_complete()) {
    SetCurrentStep(Step::kClosed);
  }

  for (auto& observer : observers_)
    observer.OnCancelRequest();
}

void AuthenticatorRequestDialogModel::ManageDevices() {
  for (auto& observer : observers_) {
    observer.OnManageDevicesClicked();
  }
}

void AuthenticatorRequestDialogModel::OnSheetModelDidChange() {
  for (auto& observer : observers_)
    observer.OnSheetModelChanged();
}

void AuthenticatorRequestDialogModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AuthenticatorRequestDialogModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AuthenticatorRequestDialogModel::OnRequestComplete() {
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
  SetCurrentStep(Step::kMissingCapability);
}

void AuthenticatorRequestDialogModel::OnNoCommonAlgorithms() {
  SetCurrentStep(Step::kMissingCapability);
}

void AuthenticatorRequestDialogModel::OnAuthenticatorStorageFull() {
  SetCurrentStep(Step::kStorageFull);
}

void AuthenticatorRequestDialogModel::OnUserConsentDenied() {
  if (use_location_bar_bubble_) {
    // Do not show a page-modal retry error sheet if the user cancelled out of
    // their platform authenticator while displaying the location bar bubble UI.
    // Instead, retry silently.
    // TODO(nsatragno): we should retry for cross platform authenticators as
    // well. However, hiding the dialog after it's been shown locks the page
    // (see crbug.com/1247338).
    StartOver();
    return;
  }
  SetCurrentStep(Step::kErrorInternalUnrecognized);
}

bool AuthenticatorRequestDialogModel::OnWinUserCancelled() {
#if BUILDFLAG(IS_WIN)
  // If the native Windows API was triggered immediately (i.e. before any Chrome
  // dialog) then start the request over (once) if the user cancels the Windows
  // UI and there are other options in Chrome's UI.
  if (!have_restarted_due_to_windows_cancel_) {
    bool have_other_option = std::any_of(
        mechanisms_.begin(), mechanisms_.end(), [](const Mechanism& m) -> bool {
          return absl::holds_alternative<Mechanism::Phone>(m.type) ||
                 absl::holds_alternative<Mechanism::AddPhone>(m.type);
        });
    bool windows_was_priority = std::any_of(
        mechanisms_.begin(), mechanisms_.end(), [](const Mechanism& m) -> bool {
          return m.priority &&
                 absl::holds_alternative<Mechanism::WindowsAPI>(m.type);
        });
    if (have_other_option && windows_was_priority) {
      have_restarted_due_to_windows_cancel_ = true;
      StartOver();
      return true;
    }
  }
#endif

  return false;
}

void AuthenticatorRequestDialogModel::OnBluetoothPoweredStateChanged(
    bool powered) {
  transport_availability_.is_ble_powered = powered;

  for (auto& observer : observers_)
    observer.OnBluetoothPoweredStateChanged();

  // For the manual flow, the user has to click the "next" button explicitly.
  if (current_step() == Step::kBlePowerOnAutomatic)
    ContinueWithFlowAfterBleAdapterPowered();
}

void AuthenticatorRequestDialogModel::SetRequestCallback(
    RequestCallback request_callback) {
  request_callback_ = request_callback;
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
  HideDialogAndDispatchToNativeWindowsApi();
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
  if (!authenticator.AuthenticatorTransport()) {
#if BUILDFLAG(IS_WIN)
    DCHECK_EQ(authenticator.GetType(),
              device::FidoAuthenticator::Type::kWinNative);
#endif  // BUILDFLAG(IS_WIN)
    return;
  }

  AuthenticatorReference authenticator_reference(
      authenticator.GetId(), *authenticator.AuthenticatorTransport());

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
  if (preselected_account_) {
    for (auto& response : responses) {
      if (response.user_entity == preselected_account_) {
        std::move(callback).Run(std::move(response));
        return;
      }
    }
    // The user selected an account that was not part of the responses. This
    // shouldn't really happen, cancel the request.
    Cancel();
    return;
  }
  ephemeral_state_.responses_ = std::move(responses);
  ephemeral_state_.creds_ = {};
  for (const auto& response : ephemeral_state_.responses_) {
    ephemeral_state_.creds_.emplace_back(device::DiscoverableCredentialMetadata(
        response.credential->id, *response.user_entity));
  }
  selection_callback_ = std::move(callback);
  SetCurrentStep(Step::kSelectAccount);
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
    const std::vector<uint8_t>& id) {
  for (const auto& cred : creds()) {
    if (cred.user.id == id) {
      preselected_account_ = std::move(cred.user);
      ephemeral_state_.creds_.clear();
      HideDialogAndDispatchToPlatformAuthenticator();
      return;
    }
  }
  NOTREACHED();
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

absl::optional<size_t> AuthenticatorRequestDialogModel::current_mechanism()
    const {
  return current_mechanism_;
}

void AuthenticatorRequestDialogModel::ContactPhoneForTesting(
    const std::string& name) {
  ContactPhone(name, /*mechanism_index=*/0);
}

void AuthenticatorRequestDialogModel::StartTransportFlowForTesting(
    AuthenticatorTransport transport) {
  StartGuidedFlowForTransport(transport, /*mechanism_index=*/0);
}

void AuthenticatorRequestDialogModel::SetCurrentStepForTesting(Step step) {
  SetCurrentStep(step);
}

bool AuthenticatorRequestDialogModel::cable_should_suggest_usb() const {
  switch (experiment_server_link_sheet_) {
    case AuthenticatorRequestDialogModel::ExperimentServerLinkSheet::CONTROL:
    case AuthenticatorRequestDialogModel::ExperimentServerLinkSheet::ARM_2:
      return base::Contains(transport_availability_.available_transports,
                            AuthenticatorTransport::kAndroidAccessory);
    case AuthenticatorRequestDialogModel::ExperimentServerLinkSheet::ARM_3:
    case AuthenticatorRequestDialogModel::ExperimentServerLinkSheet::ARM_4:
      return true;
    case AuthenticatorRequestDialogModel::ExperimentServerLinkSheet::ARM_5:
    case AuthenticatorRequestDialogModel::ExperimentServerLinkSheet::ARM_6:
      return false;
  }
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

void AuthenticatorRequestDialogModel::GetCredentialListForConditionalUi(
    base::OnceCallback<void(
        const std::vector<device::DiscoverableCredentialMetadata>&)> callback) {
  if (current_step() == Step::kLocationBarBubble) {
    std::move(callback).Run(ephemeral_state_.creds_);
    return;
  }

  conditional_ui_user_list_callback_ = std::move(callback);
}

void AuthenticatorRequestDialogModel::set_cable_transport_info(
    absl::optional<bool> extension_is_v2,
    std::vector<PairedPhone> paired_phones,
    base::RepeatingCallback<void(size_t)> contact_phone_callback,
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
  std::transform(paired_phones_.begin(), paired_phones_.end(),
                 std::back_inserter(names),
                 [](const PairedPhone& phone) -> const std::string& {
                   return phone.name;
                 });
  names.erase(std::unique(names.begin(), names.end()), names.end());
  return names;
}

void AuthenticatorRequestDialogModel::ReplaceCredListForTesting(
    std::vector<device::DiscoverableCredentialMetadata> creds) {
  ephemeral_state_.creds_ = std::move(creds);
}

absl::optional<device::PublicKeyCredentialUserEntity>
AuthenticatorRequestDialogModel::GetPreselectedAccountForTesting() {
  return preselected_account_;
}

base::WeakPtr<AuthenticatorRequestDialogModel>
AuthenticatorRequestDialogModel::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void AuthenticatorRequestDialogModel::SetCurrentStep(Step step) {
  if (!started_) {
    // Dialog isn't showing yet. Remember to show this step when it appears.
    pending_step_ = step;
    return;
  }

  current_step_ = step;
  for (auto& observer : observers_)
    observer.OnStepTransition();
}

void AuthenticatorRequestDialogModel::StartGuidedFlowForTransport(
    AuthenticatorTransport transport,
    size_t mechanism_index) {
  current_mechanism_ = mechanism_index;

  DCHECK(current_step() == Step::kMechanismSelection ||
         current_step() == Step::kUsbInsertAndActivate ||
         current_step() == Step::kCableActivate ||
         current_step() == Step::kAndroidAccessory ||
         current_step() == Step::kNotStarted);
  switch (transport) {
    case AuthenticatorTransport::kUsbHumanInterfaceDevice:
      SetCurrentStep(Step::kUsbInsertAndActivate);
      break;
    case AuthenticatorTransport::kInternal:
      StartPlatformAuthenticatorFlow();
      break;
    case AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy:
      EnsureBleAdapterIsPoweredAndContinueWithStep(Step::kCableActivate);
      break;
    case AuthenticatorTransport::kAndroidAccessory:
      SetCurrentStep(Step::kAndroidAccessory);
      break;
    default:
      break;
  }
}

void AuthenticatorRequestDialogModel::StartGuidedFlowForAddPhone(
    size_t mechanism_index) {
  current_mechanism_ = mechanism_index;
  EnsureBleAdapterIsPoweredAndContinueWithStep(Step::kCableV2QRCode);
}

void AuthenticatorRequestDialogModel::StartWinNativeApi(
    size_t mechanism_index) {
  DCHECK(transport_availability_.has_win_native_api_authenticator);
  current_mechanism_ = mechanism_index;

  if (resident_key_requirement() !=
          device::ResidentKeyRequirement::kDiscouraged &&
      !transport_availability_.win_native_ui_shows_resident_credential_notice) {
    SetCurrentStep(Step::kResidentCredentialConfirmation);
  } else {
    HideDialogAndDispatchToNativeWindowsApi();
  }
}

void AuthenticatorRequestDialogModel::ContactPhone(const std::string& name,
                                                   size_t mechanism_index) {
  current_mechanism_ = mechanism_index;

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

    BleEvent event;
    if (transport_availability()->can_power_on_ble_adapter) {
      event = BleEvent::kNeedsPowerAuto;
      SetCurrentStep(Step::kBlePowerOnAutomatic);
    } else {
      event = BleEvent::kNeedsPowerManual;
      SetCurrentStep(Step::kBlePowerOnManual);
    }
    base::UmaHistogramEnumeration("WebAuthentication.BLEUserEvents", event);
    return;
  }

  base::UmaHistogramEnumeration("WebAuthentication.BLEUserEvents",
                                BleEvent::kAlreadyPowered);
  ContactPhoneAfterBleIsPowered(std::move(name));
}

void AuthenticatorRequestDialogModel::ContactPhoneAfterBleIsPowered(
    std::string name) {
  ContactNextPhoneByName(name);
  SetCurrentStep(Step::kCableActivate);
}

void AuthenticatorRequestDialogModel::StartLocationBarBubbleRequest() {
  ephemeral_state_.creds_ = {};
  for (const auto& cred :
       transport_availability_.recognized_platform_authenticator_credentials) {
    ephemeral_state_.creds_.emplace_back(cred);
  }

  if (conditional_ui_user_list_callback_) {
    std::move(conditional_ui_user_list_callback_).Run(ephemeral_state_.creds_);
  }

  SetCurrentStep(Step::kLocationBarBubble);
}

void AuthenticatorRequestDialogModel::DispatchRequestAsync(
    AuthenticatorReference* authenticator) {
  // Dispatching to the same authenticator twice may result in unexpected
  // behavior.
  if (authenticator->dispatched) {
    return;
  }

  DispatchRequestAsyncInternal(authenticator->authenticator_id);
  authenticator->dispatched = true;
}

void AuthenticatorRequestDialogModel::DispatchRequestAsyncInternal(
    const std::string& authenticator_id) {
  if (!request_callback_)
    return;

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(request_callback_, authenticator_id));
}

void AuthenticatorRequestDialogModel::ContactNextPhoneByName(
    const std::string& name) {
  bool found_name = false;
  for (size_t i = 0; i != paired_phones_.size(); i++) {
    const PairedPhone& phone = paired_phones_[i];
    if (phone.name == name) {
      found_name = true;
      if (!paired_phones_contacted_[i]) {
        paired_phones_contacted_[i] = true;
        contact_phone_callback_.Run(phone.contact_id);
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

void AuthenticatorRequestDialogModel::PopulateMechanisms(
    bool prefer_native_api) {
  const bool is_get_assertion = transport_availability_.request_type ==
                                device::FidoRequestType::kGetAssertion;
  // priority_transport contains the transport that should be activated
  // immediately, if this is a getAssertion.
  absl::optional<AuthenticatorTransport> priority_transport;

  if (base::Contains(transport_availability_.available_transports,
                     AuthenticatorTransport::kInternal) &&
      is_get_assertion &&
      transport_availability_.has_platform_authenticator_credential ==
          device::FidoRequestHandlerBase::RecognizedCredential::
              kHasRecognizedCredential) {
    priority_transport = AuthenticatorTransport::kInternal;
  }

  std::vector<AuthenticatorTransport> transports_to_list_if_active = {
      AuthenticatorTransport::kUsbHumanInterfaceDevice,
      AuthenticatorTransport::kInternal,
  };

  const auto kCable = AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy;
  bool include_add_phone_option = false;

  if (cable_ui_type_) {
    switch (*cable_ui_type_) {
      case AuthenticatorRequestDialogModel::CableUIType::CABLE_V2_2ND_FACTOR:
        if (base::Contains(transport_availability_.available_transports,
                           kCable)) {
          include_add_phone_option = true;
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
          if (!priority_transport) {
            priority_transport = kCable;
          }

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

  // The Windows API option comes first so that it gets focus and people can
  // select it by simply hitting enter.
  if (win_native_api_enabled()) {
    const std::u16string desc = l10n_util::GetStringUTF16(
        IDS_WEBAUTHN_TRANSPORT_POPUP_DIFFERENT_AUTHENTICATOR_WIN);
    mechanisms_.emplace_back(
        Mechanism::WindowsAPI(/*unused*/ true), desc, desc,
        GetTransportIcon(AuthenticatorTransport::kUsbHumanInterfaceDevice),
        base::BindRepeating(&AuthenticatorRequestDialogModel::StartWinNativeApi,
                            base::Unretained(this), mechanisms_.size()),
        // The Windows API should have priority when requested unless caBLE does
        // because it's v1 or server-link.
        !priority_transport.has_value() &&
            (prefer_native_api ||
             (!include_add_phone_option && paired_phone_names().empty())));
  }

  if (include_add_phone_option) {
    const std::u16string label =
        l10n_util::GetStringUTF16(IDS_WEBAUTHN_CABLEV2_ADD_PHONE);
    mechanisms_.emplace_back(
        Mechanism::AddPhone(), label, label, &kQrcodeGeneratorIcon,
        base::BindRepeating(
            &AuthenticatorRequestDialogModel::StartGuidedFlowForAddPhone,
            base::Unretained(this), mechanisms_.size()),
        /* is_priority= */ false);
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
            base::Unretained(this), transport, mechanisms_.size()),
        transport_availability_.request_type ==
                device::FidoRequestType::kGetAssertion &&
            priority_transport.has_value() && *priority_transport == transport);
  }

  if (base::Contains(transport_availability_.available_transports, kCable)) {
    for (const auto& phone_name : paired_phone_names()) {
      const std::u16string name16 = base::UTF8ToUTF16(phone_name);
      static constexpr size_t kMaxLongNameChars = 50;
      static constexpr size_t kMaxShortNameChars = 30;
      std::u16string long_name, short_name;
      gfx::ElideString(name16, kMaxLongNameChars, &long_name);
      gfx::ElideString(name16, kMaxShortNameChars, &short_name);

      mechanisms_.emplace_back(
          Mechanism::Phone(phone_name), std::move(long_name),
          std::move(short_name), &kSmartphoneIcon,
          base::BindRepeating(&AuthenticatorRequestDialogModel::ContactPhone,
                              base::Unretained(this), phone_name,
                              mechanisms_.size()),
          /*priority=*/false);
    }
  }

  // At most one mechanism has priority.
  DCHECK_LE(std::count_if(mechanisms_.begin(), mechanisms_.end(),
                          [](const Mechanism& m) { return m.priority; }),
            1);
}

void AuthenticatorRequestDialogModel::
    HideDialogAndDispatchToPlatformAuthenticator() {
  HideDialog();

  auto& authenticators =
      ephemeral_state_.saved_authenticators_.authenticator_list();
  auto platform_authenticator_it =
      std::find_if(authenticators.begin(), authenticators.end(),
                   [](const auto& authenticator) {
                     return authenticator.transport ==
                            device::FidoTransportProtocol::kInternal;
                   });

  if (platform_authenticator_it == authenticators.end()) {
    return;
  }

  DispatchRequestAsync(&*platform_authenticator_it);
}
