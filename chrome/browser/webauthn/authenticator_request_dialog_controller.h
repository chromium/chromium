// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_CONTROLLER_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/authenticator_transport.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "components/webauthn/core/browser/passkey_model_change.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/global_routing_id.h"
#include "third_party/blink/public/mojom/credentialmanagement/credential_type_flags.mojom.h"

class Profile;

namespace content {
class RenderFrameHost;
}  // namespace content

// Encapsulates the logic behind the WebAuthn UI flow.
// flow. This is essentially a state machine going through the states defined in
// the `Step` enumeration.
class AuthenticatorRequestDialogController
    : public AuthenticatorRequestDialogModel::Observer,
      public webauthn::PasskeyModel::Observer {
 public:
  using RequestCallback = device::FidoRequestHandlerBase::RequestCallback;
  using BlePermissionCallback = base::RepeatingCallback<void(
      device::FidoRequestHandlerBase::BlePermissionCallback)>;

  AuthenticatorRequestDialogController(
      AuthenticatorRequestDialogModel* model,
      content::RenderFrameHost* render_frame_host);

  AuthenticatorRequestDialogController(
      const AuthenticatorRequestDialogController&) = delete;
  AuthenticatorRequestDialogController& operator=(
      const AuthenticatorRequestDialogController&) = delete;

  ~AuthenticatorRequestDialogController() override;

  AuthenticatorRequestDialogModel* model() const;

  void OnModelDestroyed(AuthenticatorRequestDialogModel* model) override;

  // Hides the dialog. A subsequent call to SetCurrentStep() will unhide it.
  void HideDialog();

  // Returns whether the UI is in a state at which the |request_| member of
  // AuthenticatorImpl has completed processing. Note that the request callback
  // is only resolved after the UI is dismissed.
  bool is_request_complete() const;

  // Starts the UX flow, by either showing the transport selection screen or
  // the guided flow for them most likely transport.
  //
  // If |is_conditional_mediation| is true, credentials will be shown on the
  // password autofill instead of the full-blown page-modal UI.
  //
  // Valid action when at step: kNotStarted.
  void StartFlow(device::FidoRequestHandlerBase::TransportAvailabilityInfo
                     transport_availability,
                 bool is_conditional_mediation);

  void StartOver() override;

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

  bool StartGuidedFlowForHint(AuthenticatorTransport transport);

  // Proceeds straight to the platform authenticator prompt. If `type` is
  // `nullopt` then it actives the default platform authenticator. Otherwise it
  // actives the platform authenticator of the given type.
  void HideDialogAndDispatchToPlatformAuthenticator(
      std::optional<device::AuthenticatorType> type = std::nullopt);

  void EnclaveEnabled() override;

  void EnclaveNeedsReauth() override;

  void OnCreatePasskeyAccepted() override;

  // Called when the transport availability info changes.
  void OnTransportAvailabilityChanged(
      device::FidoRequestHandlerBase::TransportAvailabilityInfo
          transport_availability);

  // Called when an attempt to contact a phone failed.
  void OnPhoneContactFailed(const std::string& name);

  // Called when some caBLE event (e.g. receiving a BLE message, connecting to
  // the tunnel server, etc) happens.
  void OnCableEvent(device::cablev2::Event event);

  // Called when `cable_connecting_sheet_timer_` completes.
  void OnCableConnectingTimerComplete();

  void OnRecoverSecurityDomainClosed() override;

  // StartPhonePairing triggers the display of a QR code for pairing a new
  // phone.
  void StartPhonePairing();

  // Ensures that the Bluetooth adapter is powered before executing |action|.
  //  -- If the adapter is powered, run |action| directly.
  //  -- If Chrome does not have Bluetooth permissions, show an error (macOS).
  //  -- If Chrome has not requested Bluetooth permissions yet, trigger a
  //     permission prompt (macOS).
  //  -- If the adapter is not powered, but Chrome can turn it automatically,
  //     then advanced to the flow to turn on Bluetooth automatically.
  //  -- Otherwise advanced to the manual Bluetooth power on flow.
  //
  // Valid action whenever contacting a phone or showing the QR code screen is
  // possible.
  void EnsureBleAdapterIsPoweredAndContinue(base::OnceClosure action);
  void OnBleStatusKnown(device::FidoRequestHandlerBase::BleStatus ble_status);

  void ContinueWithFlowAfterBleAdapterPowered() override;
  void PowerOnBleAdapter() override;
  void OpenBlePreferences() override;

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

  void OnOffTheRecordInterstitialAccepted() override;

  void CancelAuthenticatorRequest() override;
  void OnRequestComplete() override;

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

  // To be called when a hybrid connection fails. Returns true if the event
  // was handled.
  bool OnHybridTransportError();

  // To be called when an enclave transaction fails. Returns true if the event
  // was handled.
  bool OnEnclaveError();

  // To be called when there are no passkeys from an internal authenticator.
  // This is a rare case but can happen when the user grants passkeys permission
  // on macOS as part of a request flow and then Chromium realises that the
  // request should never have been sent to iCloud Keychain in the first place.
  bool OnNoPasskeys();

  // To be called when the Bluetooth adapter status changes.
  void BluetoothAdapterStatusChanged(
      device::FidoRequestHandlerBase::BleStatus ble_status);

  void SetRequestCallback(RequestCallback request_callback);
  void SetAccountPreselectedCallback(
      content::AuthenticatorRequestClientDelegate::AccountPreselectedCallback
          callback);
  void SetBluetoothAdapterPowerOnCallback(
      base::RepeatingClosure bluetooth_adapter_power_on_callback);
  void SetRequestBlePermissionCallback(BlePermissionCallback callback);
  void OnHavePIN(std::u16string pin) override;

  // Called when the user needs to retry user verification with the number of
  // |attempts| remaining.
  void OnRetryUserVerification(int attempts);

  void OnResidentCredentialConfirmed() override;

  // Adds or removes an authenticator to the list of known authenticators. The
  // first authenticator added with transport `kInternal` (or without a
  // transport) is considered to be the default platform authenticator.
  void AddAuthenticator(const device::FidoAuthenticator& authenticator);
  void RemoveAuthenticator(std::string_view authenticator_id);

  // SelectAccount is called to trigger an account selection dialog.
  void SelectAccount(
      std::vector<device::AuthenticatorGetAssertionResponse> responses,
      base::OnceCallback<void(device::AuthenticatorGetAssertionResponse)>
          callback);

  void OnAccountSelected(size_t index) override;

  // OnAccountPreselected is called when the user selects a discoverable
  // credential from a platform authenticator prior to providing user
  // authentication. `crededential_id` must match one of the credentials in
  // `transport_availability_.recognized_credentials`. Returns the source of the
  // credential.
  //
  // Note: it's important not to pass a reference to `credential_id` here
  // because this function clears `model_->creds`, which is where such a
  // reference would often point.
  device::AuthenticatorType OnAccountPreselected(
      const std::vector<uint8_t> credential_id);

  void OnAccountPreselectedIndex(size_t index) override;
  void SetSelectedAuthenticatorForTesting(AuthenticatorReference authenticator);
  void ContactPriorityPhone() override;

  // ContactPhoneForTesting triggers a contact for a phone with the given name.
  // Only for unittests. UI should use |mechanisms()| to enumerate the
  // user-visible mechanisms and use the callbacks therein.
  void ContactPhoneForTesting(const std::string& name);

  // Sets `priority_phone_index_` and updates the name of the priority phone in
  // `model_` accordingly.
  void SetPriorityPhoneIndex(std::optional<size_t> index);

  // StartTransportFlowForTesting moves the UI to focus on the given transport.
  // UI should use |mechanisms()| to enumerate the user-visible mechanisms and
  // use the callbacks therein.
  void StartTransportFlowForTesting(AuthenticatorTransport transport);

  // SetCurrentStepForTesting forces the model to the specified step. This
  // performs the little extra processing that this Controller does before
  // setting the model's Step. In most cases tests can set the Step on the
  // model directly.
  void SetCurrentStepForTesting(AuthenticatorRequestDialogModel::Step step);

  device::FidoRequestHandlerBase::TransportAvailabilityInfo&
  transport_availability_for_testing() {
    return transport_availability_;
  }

  ObservableAuthenticatorList& saved_authenticators() {
    return ephemeral_state_.saved_authenticators_;
  }

  const base::flat_set<AuthenticatorTransport>& available_transports() {
    return transport_availability_.available_transports;
  }

  void CollectPIN(device::pin::PINEntryReason reason,
                  device::pin::PINEntryError error,
                  uint32_t min_pin_length,
                  int attempts,
                  base::OnceCallback<void(std::u16string)> provide_pin_cb);
  void FinishCollectToken();

  void StartInlineBioEnrollment(base::OnceClosure next_callback);
  void OnSampleCollected(int bio_samples_remaining);
  void OnBioEnrollmentDone() override;

  void set_is_non_webauthn_request(bool is_non_webauthn_request) {
    is_non_webauthn_request_ = is_non_webauthn_request;
  }

  void SetHints(
      const content::AuthenticatorRequestClientDelegate::Hints& hints) {
    hints_ = hints;
  }

  void set_cable_transport_info(
      std::optional<bool> extension_is_v2,
      std::vector<std::unique_ptr<device::cablev2::Pairing>> paired_phones,
      base::RepeatingCallback<void(std::unique_ptr<device::cablev2::Pairing>)>
          contact_phone_callback,
      const std::optional<std::string>& cable_qr_string);

  bool win_native_api_enabled() const {
    return transport_availability_.has_win_native_api_authenticator;
  }

  void set_allow_icloud_keychain(bool);
  void set_should_create_in_icloud_keychain(bool);

  void set_enclave_can_be_default(bool can_be_default);

#if BUILDFLAG(IS_MAC)
  void RecordMacOsStartedHistogram();
  void RecordMacOsSuccessHistogram(device::FidoRequestType,
                                   device::AuthenticatorType);
  void set_is_active_profile_authenticator_user(bool);
  void set_has_icloud_drive_enabled(bool);
#endif

  void set_ambient_credential_types(int types);

  base::WeakPtr<AuthenticatorRequestDialogController> GetWeakPtr();

 private:
  FRIEND_TEST_ALL_PREFIXES(AuthenticatorRequestDialogControllerTest,
                           DeduplicateAccounts);

  // Contains the state that will be reset when calling StartOver(). StartOver()
  // might be called at an arbitrary point of execution.
  struct EphemeralState {
    EphemeralState();
    EphemeralState(EphemeralState&&);
    EphemeralState& operator=(EphemeralState&&);
    ~EphemeralState();

    // Stores a list of |AuthenticatorReference| values such that a request can
    // be dispatched dispatched after some UI interaction. This is useful for
    // platform authenticators (and Windows) where dispatch to the authenticator
    // immediately results in modal UI to appear.
    ObservableAuthenticatorList saved_authenticators_;

    // responses_ contains possible responses to select between after an
    // authenticator has responded to a request.
    std::vector<device::AuthenticatorGetAssertionResponse> responses_;

    // When a request has been dispatched to a platform authenticator, this
    // contains the `AuthenticatorType`. std::nullopt at all other times.
    std::optional<device::AuthenticatorType>
        dispatched_platform_authenticator_type_ = std::nullopt;

    // did_invoke_platform_despite_no_priority_mechanism_ is true if a platform
    // authenticator was triggered despite there not being a
    // `priority_mechanism_index_` set. For example, this can happen if there's
    // an allowlist match.
    bool did_invoke_platform_despite_no_priority_mechanism_ = false;
  };

  void ResetEphemeralState();

  void SetCurrentStep(AuthenticatorRequestDialogModel::Step step);

  // Requests that the step-by-step wizard flow commence, guiding the user
  // through using the Secutity Key with the given |transport|.
  //
  // Valid action when at step: kNotStarted. kMechanismSelection, and steps
  // where the other transports menu is shown, namely, kUsbInsertAndActivate,
  // kCableActivate.
  void StartGuidedFlowForTransport(AuthenticatorTransport transport);

  // Starts the flow for adding an unlisted phone by showing a QR code.
  void StartGuidedFlowForAddPhone();

  // Displays a resident-key warning if needed and then calls
  // |HideDialogAndDispatchToNativeWindowsApi|.
  void StartWinNativeApi();

  void StartICloudKeychain();
  void StartEnclave();

  // Triggers gaia account reauth to restore sync to working order.
  void ReauthForSyncRestore();

  // Contacts a paired phone. The phone is specified by name.
  void ContactPhone(const std::string& name);
  void ContactPhoneAfterOffTheRecordInterstitial(std::string name);
  void ContactPhoneAfterBleIsPowered(std::string name);

  void StartConditionalMediationRequest();

  void DispatchRequestAsync(AuthenticatorReference* authenticator);

  void ContactNextPhoneByName(const std::string& name);

  // Returns the index (into `paired_phones_`) of a phone that has been paired
  // through Chrome Sync, or std::nullopt if there isn't one.
  std::optional<size_t> GetIndexOfMostRecentlyUsedPhoneFromSync() const;

  // SortRecognizedCredentials sorts
  // `transport_availability_.recognized_credentials` into username order.
  void SortRecognizedCredentials();

  // PopulateMechanisms fills in |model_->mechanisms|.
  void PopulateMechanisms();

  // Adds a button that triggers Windows Hello with the specified string ID and
  // transport icon.
  void AddWindowsButton(int label, AuthenticatorTransport transport);

  // IndexOfPriorityMechanism returns the index, in |model_->mechanisms|, of the
  // Mechanism that should be triggered immediately, if any.
  std::optional<size_t> IndexOfPriorityMechanism();

  std::optional<size_t> IndexOfGetAssertionPriorityMechanism();
  std::optional<size_t> IndexOfMakeCredentialPriorityMechanism();

  // Sets correct step for entering GPM pin based on `gpm_pin_is_arbitrary_`.
  void PromptForGPMPin();

  // webauthn::PasskeyModel::Observer:
  void OnPasskeysChanged(
      const std::vector<webauthn::PasskeyModelChange>& changes) override;
  void OnPasskeyModelShuttingDown() override;
  void OnPasskeyModelIsReady(bool is_ready) override;

  // Update fields in `model_` based on the value of `transport_availability_`
  // and `priority_mechanism_index_`.
  void UpdateModelForTransportAvailability();

  void OnUserConfirmedPriorityMechanism() override;

  void OnChromeOSGPMRequestReady() override;

  // Returns true if this request could pick the enclave authenticator by
  // default. This only makes sense for a create() call.
  bool CanDefaultToEnclave(Profile* profile);

  // Returns the render frame host associated with this request. The render
  // frame host indirectly owns the controller, and so it should outlive it.
  content::RenderFrameHost* GetRenderFrameHost() const;

  raw_ptr<AuthenticatorRequestDialogModel> model_;

  // Identifier for the RenderFrameHost of the frame that initiated the current
  // request.
  EphemeralState ephemeral_state_;

  // is_non_webauthn_request_ is true if the current request came from Secure
  // Payment Confirmation, or from credit-card autofill.
  bool is_non_webauthn_request_ = false;

  // started_ records whether |StartFlow| has been called.
  bool started_ = false;

  // pending_step_ holds requested steps until the UI is shown. The UI is only
  // shown once the TransportAvailabilityInfo is available, but authenticators
  // may request, e.g., PIN entry prior to that.
  std::optional<AuthenticatorRequestDialogModel::Step> pending_step_;

  // after_off_the_record_interstitial_ contains the closure to run if the user
  // accepts the interstitial that warns that platform/caBLE authenticators may
  // record information even in incognito mode.
  base::OnceClosure after_off_the_record_interstitial_;

  // after_ble_adapter_powered_ contains the closure to run if the user
  // accepts the interstitial that requests to turn on the BLE adapter.
  base::OnceClosure after_ble_adapter_powered_;

  // This field is only filled out once the UX flow is started.
  device::FidoRequestHandlerBase::TransportAvailabilityInfo
      transport_availability_;

  content::AuthenticatorRequestClientDelegate::AccountPreselectedCallback
      account_preselected_callback_;
  RequestCallback request_callback_;
  base::RepeatingClosure bluetooth_adapter_power_on_callback_;

  // Triggers a permission prompt on macOS if ble_status is
  // kPendingPermissionRequest and returns the bluetooth status.
  BlePermissionCallback request_ble_permission_callback_;

  base::OnceClosure bio_enrollment_callback_;

  base::OnceCallback<void(std::u16string)> pin_callback_;

  base::OnceCallback<void(device::AuthenticatorGetAssertionResponse)>
      selection_callback_;

  // True if this request should display credentials on the password autofill
  // prompt instead of the page-modal, regular UI.
  bool use_conditional_mediation_ = false;

  // cable_extension_provided_ indicates whether the request included a caBLE
  // extension.
  bool cable_extension_provided_ = false;

  // paired_phones_ contains details of caBLEv2-paired phones from both Sync and
  // QR-based pairing. The entries are sorted by name.
  std::vector<std::unique_ptr<device::cablev2::Pairing>> paired_phones_;

  // The index, into `paired_phones_`, for the top-priority phone.
  std::optional<size_t> priority_phone_index_;

  // paired_phones_contacted_ is the same length as |paired_phones_| and
  // contains true whenever the corresponding phone as already been contacted.
  std::vector<bool> paired_phones_contacted_;

  // contact_phone_callback can be run with a pairing in order to contact the
  // indicated phone.
  base::RepeatingCallback<void(std::unique_ptr<device::cablev2::Pairing>)>
      contact_phone_callback_;

  // cable_device_ready_ is true if a CTAP-level request has been sent to a
  // caBLE device. At this point we assume that any transport errors are
  // cancellations on the device, not networking errors.
  bool cable_device_ready_ = false;

  // cable_connecting_sheet_timer_ is started when we start displaying
  // the "connecting..." sheet for a caBLE connection. To avoid flashing the UI,
  // the sheet won't be automatically replaced until it completes.
  base::OneShotTimer cable_connecting_sheet_timer_;

  // cable_connecting_ready_to_advance_ is set to true if we are ready to
  // advance the "connecting" sheet but are waiting for
  // `cable_connecting_sheet_timer_` to complete.
  bool cable_connecting_ready_to_advance_ = false;

  // allow_icloud_keychain_ is true if iCloud Keychain can be used for this
  // request. It is disabled for Secure Payment Confirmation and other non-
  // WebAuthn cases, for example.
  bool allow_icloud_keychain_ = false;

  // should_create_in_icloud_keychain is true if creation requests with
  // attachment=platform should default to iCloud Keychain rather than the
  // profile authenticator.
  bool should_create_in_icloud_keychain_ = false;

  // enclave_enabled_ is true if a "Google Password Manager" entry should be
  // offered as a mechanism for creating a credential.
  bool enclave_enabled_ = false;

  // enclave_needs_reauth_ is true if a "Reauth to use Google Password Manager"
  // entry should be offered as a mechanism for creating or using a credential.
  bool enclave_needs_reauth_ = false;

  // The RP's hints. See
  // https://w3c.github.io/webauthn/#enumdef-publickeycredentialhints
  content::AuthenticatorRequestClientDelegate::Hints hints_;

  // True when the priority mechanism was determined to be the enclave.
  bool enclave_was_priority_mechanism_ = false;

#if BUILDFLAG(IS_MAC)
  // did_record_macos_start_histogram_ is set to true if a histogram record of
  // starting the current request was made. Any later successful completion will
  // only be recorded if a start event was recorded first.
  bool did_record_macos_start_histogram_ = false;

  // is_active_profile_authenticator_user_ is true if the current profile has
  // recently used the platform authenticator on macOS that saves credentials
  // into the profile.
  bool is_active_profile_authenticator_user_ = false;

  // has_icloud_drive_enabled_ is true if the current system has iCloud Drive
  // enabled. This is used as an approximation for whether iCloud Keychain
  // syncing is enabled.
  bool has_icloud_drive_enabled_ = false;
#endif

  bool enclave_can_be_default_ = true;

  // The credential types that are being asked for in an ambient UI
  // request.
  int ambient_credential_types_ =
      static_cast<int>(blink::mojom::CredentialTypeFlags::kNone);

  const content::GlobalRenderFrameHostId frame_host_id_;

  base::ScopedObservation<webauthn::PasskeyModel,
                          webauthn::PasskeyModel::Observer>
      passkey_model_observation_{this};

  base::WeakPtrFactory<AuthenticatorRequestDialogController> weak_factory_{
      this};
};

#endif  // CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_CONTROLLER_H_
