// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_MODEL_H_
#define CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_MODEL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/webauthn/authenticator_reference.h"
#include "chrome/browser/webauthn/authenticator_transport.h"
#include "chrome/browser/webauthn/observable_authenticator_list.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"

namespace device {
class AuthenticatorGetAssertionResponse;
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
  using BlePairingCallback = device::FidoRequestHandlerBase::BlePairingCallback;
  using BleDevicePairedCallback = base::RepeatingCallback<void(std::string)>;
  using TransportAvailabilityInfo =
      device::FidoRequestHandlerBase::TransportAvailabilityInfo;

  // Defines the potential steps of the Web Authentication API request UX flow.
  enum class Step {
    // The UX flow has not started yet, the dialog should still be hidden.
    kNotStarted,

    kTransportSelection,

    // The request errored out before completing. Error will only be sent
    // after user interaction.
    kErrorNoAvailableTransports,
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

    // Touch ID.
    kTouchIdIncognitoSpeedBump,

    // Phone as a security key.
    kCableActivate,
    kCableV2Activate,
    kCableV2QRCode,

    // Authenticator Client PIN.
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

    // Account selection,
    kSelectAccount,

    // Attestation permission request.
    kAttestationPermissionRequest,
  };

  // Implemented by the dialog to observe this model and show the UI panels
  // appropriate for the current step.
  class Observer {
   public:
    // Called when the user clicks "Try Again" to restart the user flow.
    virtual void OnStartOver() {}

    // Called just before the model is destructed.
    virtual void OnModelDestroyed() = 0;

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
  };

  explicit AuthenticatorRequestDialogModel(const std::string& relying_party_id);
  ~AuthenticatorRequestDialogModel();

  void SetCurrentStep(Step step);
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

  bool should_dialog_be_closed() const {
    return current_step() == Step::kClosed;
  }
  bool should_dialog_be_hidden() const {
    return current_step() == Step::kNotStarted;
  }

  const TransportAvailabilityInfo* transport_availability() const {
    return &transport_availability_;
  }

  bool ble_adapter_is_powered() const {
    return transport_availability()->is_ble_powered;
  }

  const base::Optional<std::string>& selected_authenticator_id() const {
    return ephemeral_state_.selected_authenticator_id_;
  }

  // Starts the UX flow, by either showing the transport selection screen or
  // the guided flow for them most likely transport.
  //
  // Valid action when at step: kNotStarted.
  void StartFlow(
      TransportAvailabilityInfo transport_availability,
      base::Optional<device::FidoTransportProtocol> last_used_transport);

  // Restarts the UX flow.
  void StartOver();

  // Starts the UX flow. Tries to figure out the most likely transport to be
  // used, and starts the guided flow for that transport; or shows the manual
  // transport selection screen if the transport could not be uniquely
  // identified.
  //
  // Valid action when at step: kNotStarted.
  void StartGuidedFlowForMostLikelyTransportOrShowTransportSelection();

  // Requests that the step-by-step wizard flow commence, guiding the user
  // through using the Secutity Key with the given |transport|.
  //
  // Valid action when at step: kNotStarted.
  // kTransportSelection, and steps where the other transports menu is shown,
  // namely, kUsbInsertAndActivate, kCableActivate.
  void StartGuidedFlowForTransport(AuthenticatorTransport transport);

  // Hides the modal Chrome UI dialog and shows the native Windows WebAuthn
  // UI instead.
  void HideDialogAndDispatchToNativeWindowsApi();

  // Displays a resident-key warning if needed and then calls
  // |HideDialogAndDispatchToNativeWindowsApi|.
  void StartWinNativeApi();

  // StartPhonePairing triggers the display of a QR code for pairing a new
  // phone.
  void StartPhonePairing();

  // Ensures that the Bluetooth adapter is powered before proceeding to |step|.
  //  -- If the adapter is powered, advanced directly to |step|.
  //  -- If the adapter is not powered, but Chrome can turn it automatically,
  //     then advanced to the flow to turn on Bluetooth automatically.
  //  -- Otherwise advanced to the manual Bluetooth power on flow.
  //
  // Valid action when at step: kNotStarted, kTransportSelection, and steps
  // where the other transports menu is shown, namely, kUsbInsertAndActivate,
  // kCableActivate.
  void EnsureBleAdapterIsPoweredAndContinueWithCable();

  // Continues with the BLE/caBLE flow now that the Bluetooth adapter is
  // powered.
  //
  // Valid action when at step: kBlePowerOnManual, kBlePowerOnAutomatic.
  void ContinueWithFlowAfterBleAdapterPowered();

  // Turns on the BLE adapter automatically.
  //
  // Valid action when at step: kBlePowerOnAutomatic.
  void PowerOnBleAdapter();

  // Tries if a USB device is present -- the user claims they plugged it in.
  //
  // Valid action when at step: kUsbInsert.
  void TryUsbDevice();

  // Tries to use Touch ID -- either because the request requires it or because
  // the user told us to. May show an error for unrecognized credential, or an
  // Incognito mode interstitial, or proceed straight to the Touch ID prompt.
  //
  // Valid action when at all steps.
  void StartTouchIdFlow();

  // Proceeds straight to the Touch ID prompt.
  //
  // Valid action when at all steps.
  void HideDialogAndTryTouchId();

  // Cancels the flow as a result of the user clicking `Cancel` on the UI.
  //
  // Valid action at all steps.
  void Cancel();

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

  // To be called when the selected authenticator doesn't support any of the
  // COSEAlgorithmIdentifiers requested by the RP.
  void OnNoCommonAlgorithms();

  // To be called when the selected authenticator cannot create a resident
  // credential because of insufficient storage.
  void OnAuthenticatorStorageFull();

  // To be called when the user denies consent, e.g. by clicking "Cancel" on the
  // system Touch ID prompt.
  void OnUserConsentDenied();

  // To be called when the user clicks "Cancel" in the native Windows UI.
  // Returns true if the event was handled.
  bool OnWinUserCancelled();

  // To be called when the Bluetooth adapter powered state changes.
  void OnBluetoothPoweredStateChanged(bool powered);

  void SetRequestCallback(RequestCallback request_callback);

  void SetBluetoothAdapterPowerOnCallback(
      base::RepeatingClosure bluetooth_adapter_power_on_callback);

  void SetPINCallback(base::OnceCallback<void(std::string)> pin_callback);

  // OnHavePIN is called when the user enters a PIN in the UI.
  void OnHavePIN(const std::string& pin);

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

  void SetSelectedAuthenticatorForTesting(AuthenticatorReference authenticator);

  ObservableAuthenticatorList& saved_authenticators() {
    return ephemeral_state_.saved_authenticators_;
  }

  const base::flat_set<AuthenticatorTransport>& available_transports() {
    return transport_availability_.available_transports;
  }

  base::span<const uint8_t, device::cablev2::kQRKeySize> qr_generator_key()
      const {
    return *qr_generator_key_;
  }

  void CollectPIN(base::Optional<int> attempts,
                  base::OnceCallback<void(std::string)> provide_pin_cb);
  bool has_attempted_pin_entry() const {
    return ephemeral_state_.has_attempted_pin_entry_;
  }
  base::Optional<int> pin_attempts() const { return pin_attempts_; }

  void StartInlineBioEnrollment(base::OnceClosure next_callback);
  void OnSampleCollected(int bio_samples_remaining);
  void OnBioEnrollmentDone();
  base::Optional<int> max_bio_samples() { return max_bio_samples_; }
  base::Optional<int> bio_samples_remaining() { return bio_samples_remaining_; }

  // Flags the authenticator's internal user verification as locked.
  void set_internal_uv_locked() { uv_attempts_ = 0; }
  base::Optional<int> uv_attempts() const { return uv_attempts_; }

  void RequestAttestationPermission(base::OnceCallback<void(bool)> callback);

  const std::vector<device::AuthenticatorGetAssertionResponse>& responses() {
    return ephemeral_state_.responses_;
  }

  void set_has_attempted_pin_entry_for_testing() {
    ephemeral_state_.has_attempted_pin_entry_ = true;
  }

  void set_incognito_mode(bool incognito_mode) {
    incognito_mode_ = incognito_mode;
  }

  bool might_create_resident_credential() const {
    return might_create_resident_credential_;
  }

  void set_might_create_resident_credential(bool v) {
    might_create_resident_credential_ = v;
  }

  void set_cable_transport_info(
      bool cable_extension_provided,
      bool has_paired_phones,
      const base::Optional<std::array<uint8_t, device::cablev2::kQRKeySize>>&
          qr_generator_key);

  bool win_native_api_enabled() const {
    return transport_availability_.has_win_native_api_authenticator;
  }

  bool cable_extension_provided() const { return cable_extension_provided_; }

  const std::string& relying_party_id() const { return relying_party_id_; }

  bool offer_try_again_in_ui() const { return offer_try_again_in_ui_; }

  base::WeakPtr<AuthenticatorRequestDialogModel> GetWeakPtr();

 private:
  // Contains the state that will be reset when calling StartOver(). StartOver()
  // might be called at an arbitrary point of execution.
  struct EphemeralState {
    EphemeralState();
    ~EphemeralState();

    void Reset();

    // Represents the id of the Bluetooth authenticator that the user is trying
    // to connect to or conduct WebAuthN request to via the WebAuthN UI.
    base::Optional<std::string> selected_authenticator_id_;

    // Transport type and id of Mac TouchId and BLE authenticators are cached so
    // that the WebAuthN request for the corresponding authenticators can be
    // dispatched lazily after the user interacts with the UI element.
    ObservableAuthenticatorList saved_authenticators_;

    bool has_attempted_pin_entry_ = false;

    // responses_ contains possible accounts to select between.
    std::vector<device::AuthenticatorGetAssertionResponse> responses_;
  };

  void DispatchRequestAsync(AuthenticatorReference* authenticator);
  void DispatchRequestAsyncInternal(const std::string& authenticator_id);

  EphemeralState ephemeral_state_;

  // relying_party_id is the RP ID from Webauthn, essentially a domain name.
  const std::string relying_party_id_;

  // The current step of the request UX flow that is currently shown.
  Step current_step_ = Step::kNotStarted;

  // Determines which step to continue with once the Blueooth adapter is
  // powered. Only set while the |current_step_| is either kBlePowerOnManual,
  // kBlePowerOnAutomatic.
  base::Optional<Step> next_step_once_ble_powered_;

  base::ObserverList<Observer>::Unchecked observers_;

  // These fields are only filled out when the UX flow is started.
  TransportAvailabilityInfo transport_availability_;
  base::Optional<device::FidoTransportProtocol> last_used_transport_;

  RequestCallback request_callback_;
  base::RepeatingClosure bluetooth_adapter_power_on_callback_;

  base::Optional<int> max_bio_samples_;
  base::Optional<int> bio_samples_remaining_;
  base::OnceClosure bio_enrollment_callback_;

  base::OnceCallback<void(std::string)> pin_callback_;
  base::Optional<int> pin_attempts_;
  base::Optional<int> uv_attempts_;

  base::OnceCallback<void(bool)> attestation_callback_;

  // might_create_resident_credential_ records whether activating an
  // authenticator may cause a resident credential to be created. A resident
  // credential may be discovered by someone with physical access to the
  // authenticator and thus has privacy implications.
  bool might_create_resident_credential_ = false;

  base::OnceCallback<void(device::AuthenticatorGetAssertionResponse)>
      selection_callback_;

  bool incognito_mode_ = false;

  // offer_try_again_in_ui_ indicates whether a button to retry the request
  // should be included on the dialog sheet shown when encountering certain
  // errors.
  bool offer_try_again_in_ui_ = true;

  // cable_extension_provided_ indicates whether the request included a caBLE
  // extension.
  bool cable_extension_provided_ = false;
  // have_paired_phones_ indicates whether this profile knows of any paired
  // phones.
  bool have_paired_phones_ = false;
  base::Optional<std::array<uint8_t, device::cablev2::kQRKeySize>>
      qr_generator_key_;
  // win_native_api_already_tried_ is true if the Windows-native UI has been
  // displayed already and the user cancelled it. In this case, we shouldn't
  // jump straight to showing it again.
  bool win_native_api_already_tried_ = false;

  base::WeakPtrFactory<AuthenticatorRequestDialogModel> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorRequestDialogModel);
};

#endif  // CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_MODEL_H_
