// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_MODEL_H_
#define CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_MODEL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/strings/string_piece.h"
#include "base/types/strong_alias.h"
#include "build/build_config.h"
#include "chrome/browser/webauthn/authenticator_reference.h"
#include "chrome/browser/webauthn/authenticator_transport.h"
#include "chrome/browser/webauthn/observable_authenticator_list.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/global_routing_id.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/pin.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace device {
class AuthenticatorGetAssertionResponse;
class DiscoverableCredentialMetadata;
}

namespace gfx {
struct VectorIcon;
}

// Encapsulates the model behind the Web Authentication request dialog's UX
// flow. This is essentially a state machine going through the states defined in
// the `Step` enumeration.
//
// Ultimately, this will become an observer of the AuthenticatorRequest, and
// contain the logic to figure out which steps the user needs to take, in which
// order, to complete the authentication flow.
class AuthenticatorRequestDialogModel {
 public:
  using RequestCallback = device::FidoRequestHandlerBase::RequestCallback;
  using TransportAvailabilityInfo =
      device::FidoRequestHandlerBase::TransportAvailabilityInfo;

  // Defines the potential steps of the Web Authentication API request UX flow.
  enum class Step {
    // The UX flow has not started yet, the dialog should still be hidden.
    kNotStarted,

    // Conditionally mediated UI. No dialog is shown, instead credentials are
    // offered to the user on the password autofill prompt.
    kConditionalMediation,

    kMechanismSelection,

    // The request errored out before completing. Error will only be sent
    // after user interaction.
    kErrorNoAvailableTransports,
    kErrorNoPasskeys,
    kErrorInternalUnrecognized,

    // The request is already complete, but the error dialog should wait
    // until user acknowledgement.
    kTimedOut,
    kKeyNotRegistered,
    kKeyAlreadyRegistered,
    kMissingCapability,
    kStorageFull,

    // The request is completed, and the dialog should be closed.
    kClosed,

    // Universal Serial Bus (USB).
    kUsbInsertAndActivate,

    // Bluetooth Low Energy (BLE).
    kBlePowerOnAutomatic,
    kBlePowerOnManual,
#if BUILDFLAG(IS_MAC)
    kBlePermissionMac,
#endif

    // Let the user confirm that they want to create a credential in an
    // off-the-record browsing context. Used for platform and caBLE credentials,
    // where we feel that it's perhaps not obvious that something will be
    // recorded.
    kOffTheRecordInterstitial,

    // Phone as a security key.
    kCableActivate,
    kAndroidAccessory,
    kCableV2QRCode,

    // Authenticator Client PIN.
    kClientPinChange,
    kClientPinEntry,
    kClientPinSetup,
    kClientPinTapAgain,
    kClientPinErrorSoftBlock,
    kClientPinErrorHardBlock,
    kClientPinErrorAuthenticatorRemoved,

    // Authenticator Internal User Verification
    kInlineBioEnrollment,
    kRetryInternalUserVerification,

    // Confirm user consent to create a resident credential. Used prior to
    // triggering Windows-native APIs when Windows itself won't show any
    // notice about resident credentials.
    kResidentCredentialConfirmation,

    // Account selection. This occurs prior to performing user verification for
    // platform authenticators ("pre-select"), or afterwards for USB security
    // keys. In each mode, there are different sheets for confirming a single
    // available credential and choosing one from a list of multiple options.
    kSelectAccount,
    kSelectSingleAccount,
    kPreSelectAccount,
    kPreSelectSingleAccount,

    // Attestation permission requests.
    kAttestationPermissionRequest,
    kEnterpriseAttestationPermissionRequest,

    kCreatePasskey,
  };

  // Implemented by the dialog to observe this model and show the UI panels
  // appropriate for the current step.
  class Observer : public base::CheckedObserver {
   public:
    // Called when the user clicks "Try Again" to restart the user flow.
    virtual void OnStartOver() {}

    // Called just before the model is destructed.
    virtual void OnModelDestroyed(AuthenticatorRequestDialogModel* model) = 0;

    // Called when the UX flow has navigated to a different step, so the UI
    // should update.
    virtual void OnStepTransition() {}

    // Called when the model corresponding to the current sheet of the UX flow
    // was updated, so UI should update.
    virtual void OnSheetModelChanged() {}

    // Called when the power state of the Bluetooth adapter has changed.
    virtual void OnBluetoothPoweredStateChanged() {}

    // Called when the user cancelled WebAuthN request by clicking the
    // "cancel" button or the back arrow in the UI dialog.
    virtual void OnCancelRequest() {}

    // Called when the user clicks “Manage Devices” to manage their phones.
    virtual void OnManageDevicesClicked() {}
  };

  // A Mechanism is a user-visable method of authenticating. It might be a
  // transport (such as USB), a platform authenticator, a phone, or even a
  // delegation to a platform API. Mechanisms are listed in the UI for the
  // user to select between.
  struct Mechanism {
    // These types describe the type of Mechanism.
    using Transport =
        base::StrongAlias<class TransportTag, AuthenticatorTransport>;
    using WindowsAPI = base::StrongAlias<class WindowsAPITag,
                                         bool /* unused, but cannot be void */>;
    using Phone = base::StrongAlias<class PhoneTag, std::string>;
    using AddPhone = base::StrongAlias<class AddPhoneTag,
                                       bool /* unused, but cannot be void */>;
    using Type = absl::variant<Transport, WindowsAPI, Phone, AddPhone>;

    Mechanism(Type type,
              std::u16string name,
              std::u16string short_name,
              const gfx::VectorIcon& icon,
              base::RepeatingClosure callback,
              bool is_priority);
    ~Mechanism();
    Mechanism(Mechanism&&);
    Mechanism(const Mechanism&) = delete;
    Mechanism& operator=(const Mechanism&) = delete;

    const Type type;
    const std::u16string name;
    const std::u16string short_name;
    const raw_ref<const gfx::VectorIcon> icon;
    const base::RepeatingClosure callback;
    // priority is true if this mechanism should be activated immediately.
    // Only a single Mechanism in a list should have priority.
    const bool priority;
  };

  // PairedPhone represents a paired caBLEv2 device.
  struct PairedPhone {
    PairedPhone() = delete;
    PairedPhone(const PairedPhone&);
    PairedPhone(
        const std::string& name,
        size_t contact_id,
        const std::array<uint8_t, device::kP256X962Length> public_key_x962);
    ~PairedPhone();

    PairedPhone& operator=(const PairedPhone&);

    static bool CompareByName(const PairedPhone& a, const PairedPhone& b);

    // name is the human-friendly name of the phone. It may be unreasonably
    // long, however, and should be elided to fit within UIs.
    std::string name;
    // contact_id is an ID that can be passed to the FidoDiscoveryFactory's
    // |get_cable_contact_callback| callback in order to trigger a notification
    // to this phone.
    size_t contact_id;
    // public_key_x962 is the phone's public key.
    std::array<uint8_t, device::kP256X962Length> public_key_x962;
  };

  // CableUIType enumerates the different types of caBLE UI that we've ended
  // up with.
  enum class CableUIType {
    CABLE_V1,
    CABLE_V2_SERVER_LINK,
    CABLE_V2_2ND_FACTOR,
  };

  // ExperimentServerLinkSheet controls the the arms of an experiment to tweak
  // the behaviour and visibility of buttons on the server-link sheet.
  enum class ExperimentServerLinkSheet {
    CONTROL = 1,
    ARM_2 = 2,
    ARM_3 = 3,
    ARM_4 = 4,
    ARM_5 = 5,
    ARM_6 = 6,
  };

  // ExperimentServerLinkTitle enumerates the arms of an experiment to tweak the
  // title on the server-link sheet.
  enum class ExperimentServerLinkTitle {
    CONTROL = 11,
    UNLOCK_YOUR_PHONE = 12,
  };

  explicit AuthenticatorRequestDialogModel(
      content::RenderFrameHost* render_frame_host);

  AuthenticatorRequestDialogModel(const AuthenticatorRequestDialogModel&) =
      delete;
  AuthenticatorRequestDialogModel& operator=(
      const AuthenticatorRequestDialogModel&) = delete;

  ~AuthenticatorRequestDialogModel();

  Step current_step() const { return current_step_; }

  // Hides the dialog. A subsequent call to SetCurrentStep() will unhide it.
  void HideDialog();

  // Returns whether the UI is in a state at which the |request_| member of
  // AuthenticatorImpl has completed processing. Note that the request callback
  // is only resolved after the UI is dismissed.
  bool is_request_complete() const {
    return current_step() == Step::kTimedOut ||
           current_step() == Step::kKeyNotRegistered ||
           current_step() == Step::kKeyAlreadyRegistered ||
           current_step() == Step::kMissingCapability ||
           current_step() == Step::kClosed;
  }

  // Returns whether the visible dialog should be closed. This usually means
  // that the request has finished, or that we are in a step that does not
  // involve showing UI.
  bool should_dialog_be_closed() const {
    return current_step() == Step::kClosed ||
           current_step() == Step::kNotStarted ||
           current_step() == Step::kConditionalMediation;
  }
  const TransportAvailabilityInfo* transport_availability() const {
    return &transport_availability_;
  }

  bool ble_adapter_is_powered() const {
    return transport_availability()->is_ble_powered;
  }

  const absl::optional<std::string>& selected_authenticator_id() const {
    return ephemeral_state_.selected_authenticator_id_;
  }

  const absl::optional<std::string>& selected_phone_name() const {
    return ephemeral_state_.selected_phone_name_;
  }

  // Starts the UX flow, by either showing the transport selection screen or
  // the guided flow for them most likely transport.
  //
  // If |is_conditional_mediation| is true, credentials will be shown on the
  // password autofill instead of the full-blown page-modal UI.
  //
  // |prefer_native_api| indicates that the UI should jump directly to the
  // system WebAuthn UI if there's no better option. This is currently only
  // meaningful on Windows, but the parameter exists on all platforms to avoid
  // too much #ifdef soup.
  //
  // Valid action when at step: kNotStarted.
  void StartFlow(TransportAvailabilityInfo transport_availability,
                 bool is_conditional_mediation,
                 bool prefer_native_api);

  // Restarts the UX flow.
  void StartOver();

  // Starts a modal WebAuthn flow (i.e. what you normally get if you call
  // WebAuthn with no mediation parameter) from a conditional request.
  //
  // Valid action when at step: kConditionalMediation.
  void TransitionToModalWebAuthnRequest();

  // Starts the UX flow. Tries to figure out the most likely transport to be
  // used, and starts the guided flow for that transport; or shows the manual
  // transport selection screen if the transport could not be uniquely
  // identified.
  //
  // Valid action when at step: kNotStarted.
  void StartGuidedFlowForMostLikelyTransportOrShowMechanismSelection();

  // Hides the modal Chrome UI dialog and shows the native Windows WebAuthn
  // UI instead.
  void HideDialogAndDispatchToNativeWindowsApi();

  // Proceeds straight to the platform authenticator prompt.
  void HideDialogAndDispatchToPlatformAuthenticator();

  // Called when an attempt to contact a phone failed.
  void OnPhoneContactFailed(const std::string& name);

  // StartPhonePairing triggers the display of a QR code for pairing a new
  // phone.
  void StartPhonePairing();

  // Ensures that the Bluetooth adapter is powered before proceeding to |step|.
  //  -- If the adapter is powered, advanced directly to |step|.
  //  -- If the adapter is not powered, but Chrome can turn it automatically,
  //     then advanced to the flow to turn on Bluetooth automatically.
  //  -- Otherwise advanced to the manual Bluetooth power on flow.
  //
  // Valid action when at step: kNotStarted, kMechanismSelection, and steps
  // where the other transports menu is shown, namely, kUsbInsertAndActivate,
  // kCableActivate.
  void EnsureBleAdapterIsPoweredAndContinueWithStep(Step step);

  // Continues with the BLE/caBLE flow now that the Bluetooth adapter is
  // powered.
  //
  // Valid action when at step: kBlePowerOnManual, kBlePowerOnAutomatic.
  void ContinueWithFlowAfterBleAdapterPowered();

  // Turns on the BLE adapter automatically.
  //
  // Valid action when at step: kBlePowerOnAutomatic.
  void PowerOnBleAdapter();

  // Open the system dialog to grant BLE permission to Chrome.
  //
  // Valid action when at step: kBlePermissionMac.
  void OpenBlePreferences();

  // Tries if a USB device is present -- the user claims they plugged it in.
  //
  // Valid action when at step: kUsbInsert.
  void TryUsbDevice();

  // Tries to dispatch to the platform authenticator -- either because the
  // request requires it or because the user told us to. May show an error for
  // unrecognized credential, or an Incognito mode interstitial, or proceed
  // straight to the platform authenticator prompt.
  //
  // Valid action when at all steps.
  void StartPlatformAuthenticatorFlow();

  // OnOffTheRecordInterstitialAccepted is called when the user accepts the
  // interstitial that warns that platform/caBLE authenticators may record
  // information even in incognito mode.
  void OnOffTheRecordInterstitialAccepted();

  // Show guidance about caBLE USB fallback.
  void ShowCableUsbFallback();

  // Show caBLE activation sheet.
  void ShowCable();

  // Cancels the flow as a result of the user clicking `Cancel` on the UI.
  //
  // Valid action at all steps.
  void Cancel();

  // Opens a tab to the settings page for managing phones as security keys.
  void ManageDevices();

  // Called by the AuthenticatorRequestSheetModel subclasses when their state
  // changes, which will trigger notifying observers of OnSheetModelChanged.
  void OnSheetModelDidChange();

  // The |observer| must either outlive the object, or unregister itself on its
  // destruction.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // To be called when the Web Authentication request is complete.
  void OnRequestComplete();

  // To be called when Web Authentication request times-out.
  void OnRequestTimeout();

  // To be called when the user activates a security key that does not recognize
  // any of the allowed credentials (during a GetAssertion request).
  void OnActivatedKeyNotRegistered();

  // To be called when the user activates a security key that does recognize
  // one of excluded credentials (during a MakeCredential request).
  void OnActivatedKeyAlreadyRegistered();

  // To be called when the selected authenticator cannot currently handle PIN
  // requests because it needs a power-cycle due to too many failures.
  void OnSoftPINBlock();

  // To be called when the selected authenticator must be reset before
  // performing any PIN operations because of too many failures.
  void OnHardPINBlock();

  // To be called when the selected authenticator was removed while
  // waiting for a PIN to be entered.
  void OnAuthenticatorRemovedDuringPINEntry();

  // To be called when the selected authenticator doesn't have the requested
  // resident key capability.
  void OnAuthenticatorMissingResidentKeys();

  // To be called when the selected authenticator doesn't have the requested
  // user verification capability.
  void OnAuthenticatorMissingUserVerification();

  // To be called when the selected authenticator doesn't have the requested
  // large blob capability.
  void OnAuthenticatorMissingLargeBlob();

  // To be called when the selected authenticator doesn't support any of the
  // COSEAlgorithmIdentifiers requested by the RP.
  void OnNoCommonAlgorithms();

  // To be called when the selected authenticator cannot create a resident
  // credential because of insufficient storage.
  void OnAuthenticatorStorageFull();

  // To be called when the user denies consent, e.g. by canceling out of the
  // system's platform authenticator prompt.
  void OnUserConsentDenied();

  // To be called when the user clicks "Cancel" in the native Windows UI.
  // Returns true if the event was handled.
  bool OnWinUserCancelled();

  // To be called when the Bluetooth adapter powered state changes.
  void OnBluetoothPoweredStateChanged(bool powered);

  void SetRequestCallback(RequestCallback request_callback);

  void SetAccountPreselectedCallback(
      content::AuthenticatorRequestClientDelegate::AccountPreselectedCallback
          callback);

  void SetBluetoothAdapterPowerOnCallback(
      base::RepeatingClosure bluetooth_adapter_power_on_callback);

  // OnHavePIN is called when the user enters a PIN in the UI.
  void OnHavePIN(std::u16string pin);

  // Called when the user needs to retry user verification with the number of
  // |attempts| remaining.
  void OnRetryUserVerification(int attempts);

  // OnResidentCredentialConfirmed is called when a user accepts a dialog
  // confirming that they're happy to create a resident credential.
  void OnResidentCredentialConfirmed();

  // OnAttestationPermissionResponse is called when the user either allows or
  // disallows an attestation permission request.
  void OnAttestationPermissionResponse(bool attestation_permission_granted);

  void AddAuthenticator(const device::FidoAuthenticator& authenticator);
  void RemoveAuthenticator(base::StringPiece authenticator_id);

  // SelectAccount is called to trigger an account selection dialog.
  void SelectAccount(
      std::vector<device::AuthenticatorGetAssertionResponse> responses,
      base::OnceCallback<void(device::AuthenticatorGetAssertionResponse)>
          callback);

  // OnAccountSelected is called when one of the accounts from |SelectAccount|
  // has been picked. |index| is the index of the selected account in
  // |responses()|.
  void OnAccountSelected(size_t index);

  // OnAccountPreselected is called when the user selects a discoverable
  // credential from a platform authenticator prior to providing user
  // authentication. `crededential_id` must match one of the credentials in
  // `creds()`.
  void OnAccountPreselected(const std::vector<uint8_t>& credential_id);

  // Like `OnAccountPreselected()`, but this takes an index into `creds()`
  // instead of a credential ID.
  void OnAccountPreselectedIndex(size_t index);

  void SetSelectedAuthenticatorForTesting(AuthenticatorReference authenticator);

  base::span<const Mechanism> mechanisms() const;

  // current_mechanism returns the index into |mechanisms| of the most recently
  // activated mechanism, or nullopt if there isn't one.
  absl::optional<size_t> current_mechanism() const;

  // ContactPhoneForTesting triggers a contact for a phone with the given name.
  // Only for unittests. UI should use |mechanisms()| to enumerate the
  // user-visible mechanisms and use the callbacks therein.
  void ContactPhoneForTesting(const std::string& name);

  // StartTransportFlowForTesting moves the UI to focus on the given transport.
  // UI should use |mechanisms()| to enumerate the user-visible mechanisms and
  // use the callbacks therein.
  void StartTransportFlowForTesting(AuthenticatorTransport transport);

  // SetCurrentStepForTesting forces the model to the specified step.
  void SetCurrentStepForTesting(Step step);

  TransportAvailabilityInfo& transport_availability_for_testing() {
    return transport_availability_;
  }

  void ReplaceCredListForTesting(
      std::vector<device::DiscoverableCredentialMetadata> creds);

  ObservableAuthenticatorList& saved_authenticators() {
    return ephemeral_state_.saved_authenticators_;
  }

  const base::flat_set<AuthenticatorTransport>& available_transports() {
    return transport_availability_.available_transports;
  }

  const std::string& cable_qr_string() const { return *cable_qr_string_; }

  CableUIType cable_ui_type() const { return *cable_ui_type_; }

  // cable_should_suggest_usb returns true if the caBLE "v1" UI was triggered by
  // a caBLEv2 server-linked request and attaching a USB cable is an option.
  bool cable_should_suggest_usb() const;

  void CollectPIN(device::pin::PINEntryReason reason,
                  device::pin::PINEntryError error,
                  uint32_t min_pin_length,
                  int attempts,
                  base::OnceCallback<void(std::u16string)> provide_pin_cb);
  void FinishCollectToken();
  uint32_t min_pin_length() const { return min_pin_length_; }
  device::pin::PINEntryError pin_error() const { return pin_error_; }
  absl::optional<int> pin_attempts() const { return pin_attempts_; }

  void StartInlineBioEnrollment(base::OnceClosure next_callback);
  void OnSampleCollected(int bio_samples_remaining);
  void OnBioEnrollmentDone();
  absl::optional<int> max_bio_samples() { return max_bio_samples_; }
  absl::optional<int> bio_samples_remaining() { return bio_samples_remaining_; }

  absl::optional<int> uv_attempts() const { return uv_attempts_; }

  void RequestAttestationPermission(bool is_enterprise_attestation,
                                    base::OnceCallback<void(bool)> callback);

  const std::vector<device::DiscoverableCredentialMetadata>& creds() {
    return ephemeral_state_.creds_;
  }

  device::ResidentKeyRequirement resident_key_requirement() const {
    return transport_availability_.resident_key_requirement;
  }

  void set_cable_transport_info(
      absl::optional<bool> extension_is_v2,
      std::vector<PairedPhone> paired_phones,
      base::RepeatingCallback<void(size_t)> contact_phone_callback,
      const absl::optional<std::string>& cable_qr_string);

  bool win_native_api_enabled() const {
    return transport_availability_.has_win_native_api_authenticator;
  }

  // paired_phone_names returns a sorted, unique list of the names of paired
  // phones.
  std::vector<std::string> paired_phone_names() const;

  void set_relying_party_id(const std::string& relying_party_id) {
    relying_party_id_ = relying_party_id;
  }
  const std::string& relying_party_id() const { return relying_party_id_; }

  void set_user_entity(device::PublicKeyCredentialUserEntity user_entity) {
    user_entity_ = std::move(user_entity);
  }
  const device::PublicKeyCredentialUserEntity& user_entity() const {
    return user_entity_;
  }

  bool offer_try_again_in_ui() const { return offer_try_again_in_ui_; }

  base::WeakPtr<AuthenticatorRequestDialogModel> GetWeakPtr();

  ExperimentServerLinkTitle experiment_server_link_title_ =
      ExperimentServerLinkTitle::CONTROL;
  ExperimentServerLinkSheet experiment_server_link_sheet_ =
      ExperimentServerLinkSheet::CONTROL;

 private:
  // Contains the state that will be reset when calling StartOver(). StartOver()
  // might be called at an arbitrary point of execution.
  struct EphemeralState {
    EphemeralState();
    EphemeralState(EphemeralState&&);
    EphemeralState& operator=(EphemeralState&&);
    ~EphemeralState();

    // Represents the id of the Bluetooth authenticator that the user is trying
    // to connect to or conduct WebAuthN request to via the WebAuthN UI.
    absl::optional<std::string> selected_authenticator_id_;

    // The name of the paired phone that was passed to `ContactPhone()`. It is
    // shown on the UI sheet that prompts the user to check their phone for
    // a notification.
    absl::optional<std::string> selected_phone_name_;

    // Stores a list of |AuthenticatorReference| values such that a request can
    // be dispatched dispatched after some UI interaction. This is useful for
    // platform authenticators (and Windows) where dispatch to the authenticator
    // immediately results in modal UI to appear.
    ObservableAuthenticatorList saved_authenticators_;

    // responses_ contains possible responses to select between after an
    // authenticator has responded to a request.
    std::vector<device::AuthenticatorGetAssertionResponse> responses_;

    // creds_ contains possible credentials to select between before or after an
    // authenticator has responded to a request.
    std::vector<device::DiscoverableCredentialMetadata> creds_;
  };

  void ResetEphemeralState();

  // Can return nullptr in tests.
  content::WebContents* GetWebContents();

  void SetCurrentStep(Step step);

  // Requests that the step-by-step wizard flow commence, guiding the user
  // through using the Secutity Key with the given |transport|.
  //
  // Valid action when at step: kNotStarted. kMechanismSelection, and steps
  // where the other transports menu is shown, namely, kUsbInsertAndActivate,
  // kCableActivate.
  void StartGuidedFlowForTransport(AuthenticatorTransport transport,
                                   size_t mechanism_index);

  // Starts the flow for adding an unlisted phone by showing a QR code.
  void StartGuidedFlowForAddPhone(size_t mechanism_index);

  // Displays a resident-key warning if needed and then calls
  // |HideDialogAndDispatchToNativeWindowsApi|.
  void StartWinNativeApi(size_t mechanism_index);

  // Contacts a paired phone. The phone is specified by name.
  void ContactPhone(const std::string& name, size_t mechanism_index);
  void ContactPhoneAfterOffTheRecordInterstitial(std::string name);
  void ContactPhoneAfterBleIsPowered(std::string name);

  void StartConditionalMediationRequest();

  void DispatchRequestAsync(AuthenticatorReference* authenticator);
  void DispatchRequestAsyncInternal(const std::string& authenticator_id);

  void ContactNextPhoneByName(const std::string& name);

  // PopulateMechanisms fills in |mechanisms_|.
  //
  // |prefer_native_api| indicates that the UI should jump directly to the
  // system WebAuthn UI if there's no better option. This is currently only
  // meaningful on Windows, but the parameter exists on all platforms to avoid
  // too much #ifdef soup.
  void PopulateMechanisms(bool prefer_native_api);

  // Identifier for the RenderFrameHost of the frame that initiated the current
  // request.
  content::GlobalRenderFrameHostId frame_host_id_;

  EphemeralState ephemeral_state_;

  // relying_party_id is the RP ID from Webauthn, essentially a domain name.
  std::string relying_party_id_;

  // The current step of the request UX flow that is currently shown.
  Step current_step_ = Step::kNotStarted;

  // started_ records whether |StartFlow| has been called.
  bool started_ = false;

  // pending_step_ holds requested steps until the UI is shown. The UI is only
  // shown once the TransportAvailabilityInfo is available, but authenticators
  // may request, e.g., PIN entry prior to that.
  absl::optional<Step> pending_step_;

  // after_off_the_record_interstitial_ contains the closure to run if the user
  // accepts the interstitial that warns that platform/caBLE authenticators may
  // record information even in incognito mode.
  base::OnceClosure after_off_the_record_interstitial_;

  // after_ble_adapter_powered_ contains the closure to run if the user
  // accepts the interstitial that requests to turn on the BLE adapter.
  base::OnceClosure after_ble_adapter_powered_;

  base::ObserverList<Observer> observers_;

  // This field is only filled out once the UX flow is started.
  TransportAvailabilityInfo transport_availability_;

  content::AuthenticatorRequestClientDelegate::AccountPreselectedCallback
      account_preselected_callback_;
  RequestCallback request_callback_;
  base::RepeatingClosure bluetooth_adapter_power_on_callback_;

  absl::optional<int> max_bio_samples_;
  absl::optional<int> bio_samples_remaining_;
  base::OnceClosure bio_enrollment_callback_;

  base::OnceCallback<void(std::u16string)> pin_callback_;
  uint32_t min_pin_length_ = device::kMinPinLength;
  device::pin::PINEntryError pin_error_ = device::pin::PINEntryError::kNoError;
  absl::optional<int> pin_attempts_;
  absl::optional<int> uv_attempts_;

  base::OnceCallback<void(bool)> attestation_callback_;

  base::OnceCallback<void(device::AuthenticatorGetAssertionResponse)>
      selection_callback_;

  // True if the modal dialog is being shown right now.
  bool showing_dialog_ = false;

  // True if this request should display credentials on the password autofill
  // prompt instead of the page-modal, regular UI.
  bool use_conditional_mediation_ = false;

  // offer_try_again_in_ui_ indicates whether a button to retry the request
  // should be included on the dialog sheet shown when encountering certain
  // errors.
  bool offer_try_again_in_ui_ = true;

  // cable_extension_provided_ indicates whether the request included a caBLE
  // extension.
  bool cable_extension_provided_ = false;

  // have_restarted_due_to_windows_cancel_ is set to true if the request was
  // restarted because the UI jumped directly to the Windows UI but the user
  // hit cancel.
  bool have_restarted_due_to_windows_cancel_ = false;

  // mechanisms contains the entries that appear in the "transport" selection
  // sheet and the drop-down menu.
  std::vector<Mechanism> mechanisms_;

  // current_mechanism_ contains the index of the most recently activated
  // mechanism.
  absl::optional<size_t> current_mechanism_;

  // cable_ui_type_ contains the type of UI to display for a caBLE transaction.
  absl::optional<CableUIType> cable_ui_type_;

  // paired_phones_ contains details of caBLEv2-paired phones from both Sync and
  // QR-based pairing. The entries are sorted by name.
  std::vector<PairedPhone> paired_phones_;

  // paired_phones_contacted_ is the same length as |paired_phones_| and
  // contains true whenever the corresponding phone as already been contacted.
  std::vector<bool> paired_phones_contacted_;

  // contact_phone_callback can be run with a |PairedPhone::contact_id| in order
  // to contact the indicated phone.
  base::RepeatingCallback<void(size_t)> contact_phone_callback_;

  absl::optional<std::string> cable_qr_string_;

  // For MakeCredential requests, the PublicKeyCredentialUserEntity associated
  // with the request.
  device::PublicKeyCredentialUserEntity user_entity_;

  base::WeakPtrFactory<AuthenticatorRequestDialogModel> weak_factory_{this};
};

#endif  // CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_MODEL_H_
