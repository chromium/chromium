// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_MODEL_H_
#define CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_MODEL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/webauthn/authenticator_reference.h"
#include "chrome/browser/webauthn/authenticator_transport.h"
#include "chrome/browser/webauthn/observable_authenticator_list.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"

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
  using TransportAvailabilityInfo =
      device::FidoRequestHandlerBase::TransportAvailabilityInfo;

  // Defines the potential steps of the Web Authentication API request UX flow.
  enum class Step {
    // The UX flow has not started yet, the dialog should still be hidden.
    kNotStarted,

    kWelcomeScreen,
    kTransportSelection,

    // The request is not yet complete, and will only be after user interaction.
    kErrorNoAvailableTransports,
    kErrorInternalUnrecognized,

    // The request is already complete, but the dialog should remain open with
    // an explaining of what went wrong.
    kPostMortemTimedOut,
    kPostMortemKeyNotRegistered,
    kPostMortemKeyAlreadyRegistered,

    // The request is completed, and the dialog should be closed.
    kClosed,

    // Universal Serial Bus (USB).
    kUsbInsertAndActivate,

    // Bluetooth Low Energy (BLE).
    kBlePowerOnAutomatic,
    kBlePowerOnManual,

    kBlePairingBegin,
    kBleEnterPairingMode,
    kBleDeviceSelection,
    kBlePinEntry,

    kBleActivate,
    kBleVerifying,

    // Touch ID.
    kTouchId,

    // Phone as a security key.
    kCableActivate,
  };

  // Implemented by the dialog to observe this model and show the UI panels
  // appropriate for the current step.
  class Observer {
   public:
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

  AuthenticatorRequestDialogModel();
  ~AuthenticatorRequestDialogModel();

  void SetCurrentStep(Step step);
  Step current_step() const { return current_step_; }

  bool is_showing_post_mortem() const {
    return current_step() == Step::kPostMortemTimedOut ||
           current_step() == Step::kPostMortemKeyNotRegistered ||
           current_step() == Step::kPostMortemKeyAlreadyRegistered;
  }

  bool is_request_complete() const {
    return is_showing_post_mortem() || current_step() == Step::kClosed;
  }

  bool should_dialog_be_closed() const {
    return current_step() == Step::kClosed;
  }

  const TransportAvailabilityInfo* transport_availability() const {
    return &transport_availability_;
  }

  bool ble_adapter_is_powered() const {
    return transport_availability()->is_ble_powered;
  }

  const std::string& selected_authenticator_id() const {
    DCHECK_EQ(Step::kBlePinEntry, current_step());
    return selected_authenticator_id_;
  }

  // Starts the UX flow, by either showing the welcome screen, the transport
  // selection screen, or the guided flow for them most likely transport.
  //
  // Valid action when at step: kNotStarted.
  void StartFlow(
      TransportAvailabilityInfo transport_availability,
      base::Optional<device::FidoTransportProtocol> last_used_transport);

  // Starts the UX flow. Tries to figure out the most likely transport to be
  // used, and starts the guided flow for that transport; or shows the manual
  // transport selection screen if the transport could not be uniquely
  // identified.
  //
  // Valid action when at step: kNotStarted, kWelcomeScreen.
  void StartGuidedFlowForMostLikelyTransportOrShowTransportSelection();

  // Requests that the step-by-step wizard flow commence, guiding the user
  // through using the Secutity Key with the given |transport|.
  //
  // Valid action when at step: kNotStarted, kWelcomeScreen,
  // kTransportSelection, and steps where the other transports menu is shown,
  // namely, kUsbInsertAndActivate, kTouchId, kBleActivate, kCableActivate.
  void StartGuidedFlowForTransport(AuthenticatorTransport transport);

  // Ensures that the Bluetooth adapter is powered before proceeding to |step|.
  //  -- If the adapter is powered, advanced directly to |step|.
  //  -- If the adapter is not powered, but Chrome can turn it automatically,
  //     then advanced to the flow to turn on Bluetooth automatically.
  //  -- Otherwise advanced to the manual Bluetooth power on flow.
  //
  // Valid action when at step: kNotStarted, kWelcomeScreen,
  // kTransportSelection, and steps where the other transports menu is shown,
  // namely, kUsbInsertAndActivate, kTouchId, kBleActivate, kCableActivate.
  void EnsureBleAdapterIsPoweredBeforeContinuingWithStep(Step step);

  // Continues with the BLE/caBLE flow now that the Bluetooth adapter is
  // powered.
  //
  // Valid action when at step: kBlePowerOnManual, kBlePowerOnAutomatic.
  void ContinueWithFlowAfterBleAdapterPowered();

  // Turns on the BLE adapter automatically.
  //
  // Valid action when at step: kBlePowerOnAutomatic.
  void PowerOnBleAdapter();

  // Lets the pairing procedure start after the user learned about the need.
  //
  // Valid action when at step: kBlePairingBegin.
  void StartBleDiscovery();

  // Initiates pairing of the device that the user has chosen.
  //
  // Valid action when at step: kBleDeviceSelection.
  void InitiatePairingDevice(base::StringPiece authenticator_id);

  // Finishes pairing of the previously chosen device with the |pin| code
  // entered.
  //
  // Valid action when at step: kBlePinEntry.
  void FinishPairingWithPin(const base::string16& pin);

  // Dispatches WebAuthN request to successfully paired Bluetooth authenticator.
  //
  // Valid action when at step: kBleVerifying.
  void OnPairingSuccess(base::StringPiece authenticator_id);

  // Returns to Bluetooth device selection modal.
  //
  // Valid action when at step: kBleVerifying.
  void OnPairingFailure();

  // Tries if a USB device is present -- the user claims they plugged it in.
  //
  // Valid action when at step: kUsbInsert.
  void TryUsbDevice();

  // Tries to use Touch ID -- either because the request requires it or because
  // the user told us to.
  //
  // Valid action when at step: kTouchId.
  void StartTouchIdFlow();

  // Cancels the flow as a result of the user clicking `Cancel` on the UI.
  //
  // Valid action at all steps.
  void Cancel();

  // Backtracks in the flow as a result of the user clicking `Back` on the UI.
  //
  // Valid action at all steps.
  void Back();

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

  // To be called when the Bluetooth adapter powered state changes.
  void OnBluetoothPoweredStateChanged(bool powered);

  void SetRequestCallback(RequestCallback request_callback);

  void SetBlePairingCallback(BlePairingCallback ble_pairing_callback);

  void SetBluetoothAdapterPowerOnCallback(
      base::RepeatingClosure bluetooth_adapter_power_on_callback);

  void UpdateAuthenticatorReferenceId(base::StringPiece old_authenticator_id,
                                      std::string new_authenticator_id);
  void AddAuthenticator(const device::FidoAuthenticator& authenticator);
  void RemoveAuthenticator(base::StringPiece authenticator_id);

  void UpdateAuthenticatorReferencePairingMode(
      base::StringPiece authenticator_id,
      bool is_in_pairing_mode);

  void SetSelectedAuthenticatorForTesting(AuthenticatorReference authenticator);

  ObservableAuthenticatorList& saved_authenticators() {
    return saved_authenticators_;
  }

  const std::vector<AuthenticatorTransport>& available_transports() {
    return available_transports_;
  }

 private:
  void DispatchRequestAsync(AuthenticatorReference* authenticator,
                            base::TimeDelta delay);

  // The current step of the request UX flow that is currently shown.
  Step current_step_ = Step::kNotStarted;

  // Determines which step to continue with once the Blueooth adapter is
  // powered. Only set while the |current_step_| is either kBlePowerOnManual,
  // kBlePowerOnAutomatic.
  base::Optional<Step> next_step_once_ble_powered_;

  base::ObserverList<Observer>::Unchecked observers_;

  // These fields are only filled out when the UX flow is started.
  TransportAvailabilityInfo transport_availability_;
  std::vector<AuthenticatorTransport> available_transports_;
  base::Optional<device::FidoTransportProtocol> last_used_transport_;

  // Transport type and id of Mac TouchId and BLE authenticators are cached so
  // that the WebAuthN request for the corresponding authenticators can be
  // dispatched lazily after the user interacts with the UI element.
  ObservableAuthenticatorList saved_authenticators_;

  // Represents the id of the Bluetooth authenticator that the user is trying to
  // connect to or conduct WebAuthN request to via the WebAuthN UI.
  std::string selected_authenticator_id_;

  RequestCallback request_callback_;
  BlePairingCallback ble_pairing_callback_;
  base::RepeatingClosure bluetooth_adapter_power_on_callback_;

  base::WeakPtrFactory<AuthenticatorRequestDialogModel> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorRequestDialogModel);
};

#endif  // CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_MODEL_H_
