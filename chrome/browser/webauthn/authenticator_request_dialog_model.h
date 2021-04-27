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
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/webauthn/authenticator_reference.h"
#include "chrome/browser/webauthn/authenticator_transport.h"
#include "chrome/browser/webauthn/observable_authenticator_list.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/pin.h"
#include "device/fido/public_key_credential_user_entity.h"

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
  using TransportAvailabilityInfo =
      device::FidoRequestHandlerBase::TransportAvailabilityInfo;

  // Defines the potential steps of the Web Authentication API request UX flow.
  enum class Step {
    // The UX flow has not started yet, the dialog should still be hidden.
    kNotStarted,

    // A more subtle version of the dialog is being shown as an icon or bubble
    // on the omnibox, prompting the user to tap their security key.
    kLocationBarBubble,

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

    // Let the user confirm that they want to create a platform credential in an
    // off-the-record browsing context.
    kPlatformAuthenticatorOffTheRecordInterstitial,

    // Phone as a security key.
    kCableActivate,
    kAndroidAccessory,
    kCableV2Activate,
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

    // Account selection,
    kSelectAccount,

    // Attestation permission requests.
    kAttestationPermissionRequest,
    kEnterpriseAttestationPermissionRequest,
  };

  // Implemented by the dialog to observe this model and show the UI panels
  // appropriate for the current step.
  class Observer {
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
    return current_step() == Step::kNotStarted ||
           current_step() == Step::kLocationBarBubble;
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
  // If |use_location_bar_bubble| is true, a non-modal bubble will be displayed
  // on the location bar instead of the full-blown page-modal UI.
  //
  // Valid action when at step: kNotStarted.
  void StartFlow(
      TransportAvailabilityInfo transport_availability,
      bool use_location_bar_bubble);

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

  // Tries to dispatch to the platform authenticator -- either because the
  // request requires it or because the user told us to. May show an error for
  // unrecognized credential, or an Incognito mode interstitial, or proceed
  // straight to the platform authenticator prompt.
  //
  // Valid action when at all steps.
  void StartPlatformAuthenticatorFlow();

  // Proceeds straight to the platform authenticator prompt.
  //
  // Valid action when at all steps.
  void HideDialogAndDispatchToPlatformAuthenticator();

  // Show guidance about caBLE USB fallback.
  void ShowCableUsbFallback();

  // Show caBLE activation sheet.
  void ShowCable();

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

  void SetSelectedAuthenticatorForTesting(AuthenticatorReference authenticator);

  ObservableAuthenticatorList& saved_authenticators() {
    return ephemeral_state_.saved_authenticators_;
  }

  const base::flat_set<AuthenticatorTransport>& available_transports() {
    return transport_availability_.available_transports;
  }

  const std::string& cable_qr_string() const { return *cable_qr_string_; }

  // cable_should_suggest_usb returns true if the caBLE "v1" UI was triggered by
  // a caBLEv2 server-linked request and attaching a USB cable is an option.
  bool cable_should_suggest_usb() const;

  void CollectPIN(device::pin::PINEntryReason reason,
                  device::pin::PINEntryError error,
                  uint32_t min_pin_length,
                  int attempts,
                  base::OnceCallback<void(std::u16string)> provide_pin_cb);
  uint32_t min_pin_length() const { return min_pin_length_; }
  device::pin::PINEntryError pin_error() const { return pin_error_; }
  base::Optional<int> pin_attempts() const { return pin_attempts_; }

  void StartInlineBioEnrollment(base::OnceClosure next_callback);
  void OnSampleCollected(int bio_samples_remaining);
  void OnBioEnrollmentDone();
  base::Optional<int> max_bio_samples() { return max_bio_samples_; }
  base::Optional<int> bio_samples_remaining() { return bio_samples_remaining_; }

  base::Optional<int> uv_attempts() const { return uv_attempts_; }

  void RequestAttestationPermission(bool is_enterprise_attestation,
                                    base::OnceCallback<void(bool)> callback);

  const std::vector<device::PublicKeyCredentialUserEntity>& users() {
    return ephemeral_state_.users_;
  }

  device::ResidentKeyRequirement resident_key_requirement() const {
    return transport_availability_.resident_key_requirement;
  }

  void set_cable_transport_info(
      bool cable_extension_provided,
      bool has_paired_phones,
      const base::Optional<std::string>& cable_qr_string);

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

    // Stores a list of |AuthenticatorReference| values such that a request can
    // be dispatched dispatched after some UI interaction. This is useful for
    // platform authenticators (and Windows) where dispatch to the authenticator
    // immediately results in modal UI to appear.
    ObservableAuthenticatorList saved_authenticators_;

    // responses_ contains possible responses to select between after an
    // authenticator has responded to a request.
    std::vector<device::AuthenticatorGetAssertionResponse> responses_;

    // users_ contains possible accounts to select between before or after an
    // authenticator has responded to a request.
    std::vector<device::PublicKeyCredentialUserEntity> users_;
  };

  void StartLocationBarBubbleRequest();

  void DispatchRequestAsync(AuthenticatorReference* authenticator);
  void DispatchRequestAsyncInternal(const std::string& authenticator_id);

  EphemeralState ephemeral_state_;

  // relying_party_id is the RP ID from Webauthn, essentially a domain name.
  const std::string relying_party_id_;

  // The current step of the request UX flow that is currently shown.
  Step current_step_ = Step::kNotStarted;

  // started_ records whether |StartFlow| has been called.
  bool started_ = false;

  // pending_step_ holds requested steps until the UI is shown. The UI is only
  // shown once the TransportAvailabilityInfo is available, but authenticators
  // may request, e.g., PIN entry prior to that.
  base::Optional<Step> pending_step_;

  // Determines which step to continue with once the Blueooth adapter is
  // powered. Only set while the |current_step_| is either kBlePowerOnManual,
  // kBlePowerOnAutomatic.
  base::Optional<Step> next_step_once_ble_powered_;

  base::ObserverList<Observer>::Unchecked observers_;

  // This field is only filled out once the UX flow is started.
  TransportAvailabilityInfo transport_availability_;

  RequestCallback request_callback_;
  base::RepeatingClosure bluetooth_adapter_power_on_callback_;

  base::Optional<int> max_bio_samples_;
  base::Optional<int> bio_samples_remaining_;
  base::OnceClosure bio_enrollment_callback_;

  base::OnceCallback<void(std::u16string)> pin_callback_;
  uint32_t min_pin_length_ = device::kMinPinLength;
  device::pin::PINEntryError pin_error_ = device::pin::PINEntryError::kNoError;
  base::Optional<int> pin_attempts_;
  base::Optional<int> uv_attempts_;

  base::OnceCallback<void(bool)> attestation_callback_;

  base::OnceCallback<void(device::AuthenticatorGetAssertionResponse)>
      selection_callback_;
  base::Optional<device::PublicKeyCredentialUserEntity> preselected_account_;

  // True if this request should use the non-modal location bar bubble UI
  // instead of the page-modal, regular UI.
  bool use_location_bar_bubble_ = false;

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
  base::Optional<std::string> cable_qr_string_;
  // win_native_api_already_tried_ is true if the Windows-native UI has been
  // displayed already and the user cancelled it. In this case, we shouldn't
  // jump straight to showing it again.
  bool win_native_api_already_tried_ = false;

  base::WeakPtrFactory<AuthenticatorRequestDialogModel> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorRequestDialogModel);
};

#endif  // CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_MODEL_H_
