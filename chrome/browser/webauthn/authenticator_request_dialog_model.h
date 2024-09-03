// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_MODEL_H_
#define CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_MODEL_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/strong_alias.h"
#include "build/build_config.h"
#include "chrome/browser/webauthn/authenticator_reference.h"
#include "chrome/browser/webauthn/authenticator_transport.h"
#include "chrome/browser/webauthn/observable_authenticator_list.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "components/webauthn/core/browser/passkey_model_change.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/global_routing_id.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_types.h"
#include "device/fido/pin.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

#if BUILDFLAG(IS_MAC)
#include "crypto/scoped_lacontext.h"
#endif  // BUILDFLAG(IS_MAC)

namespace content {
class RenderFrameHost;
}  // namespace content

namespace device {
class AuthenticatorGetAssertionResponse;
class DiscoverableCredentialMetadata;
}  // namespace device

namespace gfx {
struct VectorIcon;
}

struct AccountInfo;
class AuthenticatorRequestDialogController;
class Profile;

//                ┌───────┐
//                │ View  │
//                └───────┘ Events are
//   Views call         ▲  broadcast to
//    methods,     │    │  Views, which
//  which trigger  │    │    also read
//   broadcasts    │    │   Model fields
//                 ▼
//                ┌───────┐
//                │ Model │
//                └───────┘
//                      ▲   Controller sets
//   Events are    │    │   fields and calls
// also broadcast  │    │      methods to
//     to the      │    │   broadcast events
//   Controller    ▼            to Views
//              ┌──────────┐
//              │Controller│
//              └──────────┘
//                      ▲
//                      │   Calls from other
//                      │      components
//                      │

// This lists the events on the model. Each becomes:
//   1) A virtual method on AuthenticatorRequestDialogModel::Observer.
//   2) A method on the Model that broadcasts the event to all observers.
#define AUTHENTICATOR_EVENTS                                                  \
  /* Cancels the flow as a result of the user clicking `Cancel` on the */     \
  /* UI. Valid action at all steps. */                                        \
  AUTHENTICATOR_REQUEST_EVENT_0(CancelAuthenticatorRequest)                   \
  /* Contacts the "priority" paired phone. This is the phone from sync if */  \
  /* there are a priori discovered GPM passkeys, or the first phone on the */ \
  /* list otherwise. Only valid to call if |model_->priority_phone_name| */   \
  /* contains a value. */                                                     \
  AUTHENTICATOR_REQUEST_EVENT_0(ContactPriorityPhone)                         \
  /* Continues with the BLE/caBLE flow now that the Bluetooth adapter is */   \
  /* powered. Valid action when at step: kBlePowerOnManual, */                \
  /* kBlePowerOnAutomatic. */                                                 \
  AUTHENTICATOR_REQUEST_EVENT_0(ContinueWithFlowAfterBleAdapterPowered)       \
  /* Called when the enclave authenticator is available for a request. */     \
  AUTHENTICATOR_REQUEST_EVENT_0(EnclaveEnabled)                               \
  /* Called when the enclave authenticator needs a reauth before it is */     \
  /* available for a request. */                                              \
  AUTHENTICATOR_REQUEST_EVENT_0(EnclaveNeedsReauth)                           \
  /* Called when the ChromeOS authenticator is ready to handle a pending */   \
  /* request. */                                                              \
  AUTHENTICATOR_REQUEST_EVENT_0(OnChromeOSGPMRequestReady)                    \
  AUTHENTICATOR_REQUEST_EVENT_0(OnBioEnrollmentDone)                          \
  /* Called when the power state of the Bluetooth adapter has changed. */     \
  AUTHENTICATOR_REQUEST_EVENT_0(OnBluetoothPoweredStateChanged)               \
  /* Called when the UI should update the state of the buttons. */            \
  AUTHENTICATOR_REQUEST_EVENT_0(OnButtonsStateChanged)                        \
  /* Called when the user cancelled WebAuthn request by clicking the */       \
  /* "cancel" button or the back arrow in the UI dialog. */                   \
  AUTHENTICATOR_REQUEST_EVENT_0(OnCancelRequest)                              \
  /* Called when the user picks Google Password Manager from the */           \
  /* mechanism selection sheet. */                                            \
  AUTHENTICATOR_REQUEST_EVENT_0(OnGPMSelected)                                \
  /* Called when the user accepts the create passkey sheet. */                \
  /* (But not the GPM one.) */                                                \
  AUTHENTICATOR_REQUEST_EVENT_0(OnCreatePasskeyAccepted)                      \
  /* Called when the user accepts passkey creation dialog. */                 \
  AUTHENTICATOR_REQUEST_EVENT_0(OnGPMCreatePasskey)                           \
  /* Called when the user accepts the warning dialog for creating a GPM */    \
  /* passkey in incognito mode.*/                                             \
  AUTHENTICATOR_REQUEST_EVENT_0(OnGPMConfirmOffTheRecordCreate)               \
  /* Called when the user clicks "Forgot PIN" during UV. */                   \
  AUTHENTICATOR_REQUEST_EVENT_0(OnForgotGPMPinPressed)                        \
  /* Called when the user clicks “Manage Devices” to manage their */      \
  /* phones. */                                                               \
  AUTHENTICATOR_REQUEST_EVENT_0(OnManageDevicesClicked)                       \
  /* OnOffTheRecordInterstitialAccepted is called when the user accepts */    \
  /* the interstitial that warns that platform/caBLE authenticators may */    \
  /* record information even in incognito mode. */                            \
  AUTHENTICATOR_REQUEST_EVENT_0(OnOffTheRecordInterstitialAccepted)           \
  /* Sent by GPMEnclaveController when it's ready for the UI to be */         \
  /* displayed. */                                                            \
  AUTHENTICATOR_REQUEST_EVENT_0(OnReadyForUI)                                 \
  /* Called when a user closes the MagicArch window. */                       \
  AUTHENTICATOR_REQUEST_EVENT_0(OnRecoverSecurityDomainClosed)                \
  /* To be called when the Web Authentication request is complete. */         \
  AUTHENTICATOR_REQUEST_EVENT_0(OnRequestComplete)                            \
  /* OnResidentCredentialConfirmed is called when a user accepts a dialog */  \
  /* confirming that they're happy to create a resident credential. */        \
  AUTHENTICATOR_REQUEST_EVENT_0(OnResidentCredentialConfirmed)                \
  /* Called when the model corresponding to the current sheet of the UX */    \
  /* flow was updated, so UI should update. */                                \
  AUTHENTICATOR_REQUEST_EVENT_0(OnSheetModelChanged)                          \
  AUTHENTICATOR_REQUEST_EVENT_0(OnStartOver)                                  \
  /* Called when the UX flow has navigated to a different step, so the UI */  \
  /* should update. */                                                        \
  AUTHENTICATOR_REQUEST_EVENT_0(OnStepTransition)                             \
  /* Called when the user accepts enrolling a device to use passkeys. */      \
  AUTHENTICATOR_REQUEST_EVENT_0(OnTrustThisComputer)                          \
  AUTHENTICATOR_REQUEST_EVENT_0(OnUserConfirmedPriorityMechanism)             \
  /* Open the system dialog to grant BLE permission to Chrome. Valid */       \
  /* action when at step: kBlePermissionMac. */                               \
  AUTHENTICATOR_REQUEST_EVENT_0(OpenBlePreferences)                           \
  /* Turns on the BLE adapter automatically. Valid action when at step: */    \
  /* kBlePowerOnAutomatic. */                                                 \
  AUTHENTICATOR_REQUEST_EVENT_0(PowerOnBleAdapter)                            \
  /* Show guidance about caBLE USB fallback. */                               \
  AUTHENTICATOR_REQUEST_EVENT_0(ShowCableUsbFallback)                         \
  /* Called when loading the enclave times out. */                            \
  AUTHENTICATOR_REQUEST_EVENT_0(OnLoadingEnclaveTimeout)                      \
  /* Restarts the UX flow. */                                                 \
  AUTHENTICATOR_REQUEST_EVENT_0(StartOver)                                    \
  /* Like `OnAccountPreselected()`, but this takes an index into `creds()` */ \
  /* instead of a credential ID. */                                           \
  AUTHENTICATOR_REQUEST_EVENT_1(OnAccountPreselectedIndex, size_t)            \
  /* OnAccountSelected is called when one of the accounts from  */            \
  /* |SelectAccount| has been picked. The argument is the index of the */     \
  /* selected account in |creds()|. */                                        \
  AUTHENTICATOR_REQUEST_EVENT_1(OnAccountSelected, size_t)                    \
  /* OnAttestationPermissionResponse is called when the user either */        \
  /* allows or disallows an attestation permission request. */                \
  AUTHENTICATOR_REQUEST_EVENT_1(OnAttestationPermissionResponse, bool)        \
  /* Called when the user selects a GPM passkey. */                           \
  AUTHENTICATOR_REQUEST_EVENT_1(OnGPMPasskeySelected, std::vector<uint8_t>)   \
  /* Called when the user enters the GPM pin in the UI (during initial */     \
  /* setup or authentication). */                                             \
  AUTHENTICATOR_REQUEST_EVENT_1(OnGPMPinEntered, const std::u16string&)       \
  /* Called when the user chooses an option of creating a GPM pin. The */     \
  /* argument is true if the user chooses an arbitrary PIN, and false if */   \
  /* the user chose to use a 6-digit PIN. */                                  \
  AUTHENTICATOR_REQUEST_EVENT_1(OnGPMPinOptionChanged, bool)                  \
  /* OnHavePIN is called when the user enters a PIN in the UI. */             \
  AUTHENTICATOR_REQUEST_EVENT_1(OnHavePIN, std::u16string)                    \
  /* Called when a local Touch ID prompt finishes. The first parameter is */  \
  /* true for success, false for failure. */                                  \
  /* On success, the emitter must set the model's |lacontext| to an */        \
  /* authenticated LAContext. */                                              \
  AUTHENTICATOR_REQUEST_EVENT_1(OnTouchIDComplete, bool)                      \
  /* Called when GAIA reauth has completed. The argument is the reauth */     \
  /* proof token. */                                                          \
  AUTHENTICATOR_REQUEST_EVENT_1(OnReauthComplete, std::string)                \
  /* Called just before the model is destructed. */                           \
  AUTHENTICATOR_REQUEST_EVENT_1(OnModelDestroyed,                             \
                                AuthenticatorRequestDialogModel*)             \
  /* Called when the GPM passkeys are reset successfully or not. */           \
  AUTHENTICATOR_REQUEST_EVENT_1(OnGpmPasskeysReset, bool)

// Each Step defines a unique UI state. Setting a Step causes the matching
// dialog or window to appear.
#define STEPS                                                                  \
  /* The UX flow has not started yet, the dialog should still be hidden. */    \
  F(kNotStarted)                                                               \
                                                                               \
  /* Conditionally mediated UI. No dialog is shown, instead credentials are */ \
  /* offered to the user on the password autofill prompt. */                   \
  F(kConditionalMediation)                                                     \
                                                                               \
  F(kMechanismSelection)                                                       \
                                                                               \
  /* The request errored out before completing. Error will only be sent */     \
  /* after user interaction. */                                                \
  F(kErrorNoAvailableTransports)                                               \
  F(kErrorNoPasskeys)                                                          \
  F(kErrorInternalUnrecognized)                                                \
  F(kErrorWindowsHelloNotEnabled)                                              \
                                                                               \
  /* The request is already complete, but the error dialog should wait */      \
  /* until user acknowledgement. */                                            \
  F(kTimedOut)                                                                 \
  F(kKeyNotRegistered)                                                         \
  F(kKeyAlreadyRegistered)                                                     \
  F(kMissingCapability)                                                        \
  F(kStorageFull)                                                              \
                                                                               \
  /* The request is completed, and the dialog should be closed. */             \
  F(kClosed)                                                                   \
                                                                               \
  /* Universal Serial Bus (USB). */                                            \
  F(kUsbInsertAndActivate)                                                     \
                                                                               \
  /* Bluetooth Low Energy (BLE). */                                            \
  F(kBlePowerOnAutomatic)                                                      \
  F(kBlePowerOnManual)                                                         \
  F(kBlePermissionMac)                                                         \
                                                                               \
  /* Let the user confirm that they want to create a credential in an */       \
  /* off-the-record browsing context. Used for platform and caBLE */           \
  /* credential, where we feel that it's perhaps not obvious that something */ \
  /* will be recorded. */                                                      \
  F(kOffTheRecordInterstitial)                                                 \
                                                                               \
  /* Phone as a security key. */                                               \
  F(kPhoneConfirmationSheet)                                                   \
  F(kCableActivate)                                                            \
  F(kAndroidAccessory)                                                         \
  F(kCableV2QRCode)                                                            \
  F(kCableV2Connecting)                                                        \
  F(kCableV2Connected)                                                         \
  F(kCableV2Error)                                                             \
                                                                               \
  /* Authenticator Client PIN. */                                              \
  F(kClientPinChange)                                                          \
  F(kClientPinEntry)                                                           \
  F(kClientPinSetup)                                                           \
  F(kClientPinTapAgain)                                                        \
  F(kClientPinErrorSoftBlock)                                                  \
  F(kClientPinErrorHardBlock)                                                  \
  F(kClientPinErrorAuthenticatorRemoved)                                       \
                                                                               \
  /* Authenticator Internal User Verification */                               \
  F(kInlineBioEnrollment)                                                      \
  F(kRetryInternalUserVerification)                                            \
                                                                               \
  /* Confirm user consent to create a resident credential. Used prior to */    \
  /* triggering Windows-native APIs when Windows itself won't show any */      \
  /* notice about resident credentials. */                                     \
  F(kResidentCredentialConfirmation)                                           \
                                                                               \
  /* Account selection. This occurs prior to performing user verification */   \
  /* for platform authenticators ("pre-select"), or afterwards for USB  */     \
  /* security keys. In each mode, there are different sheets for confirming */ \
  /* a single available credential and choosing one from a list of multiple */ \
  /* options. */                                                               \
  F(kSelectAccount)                                                            \
  F(kSelectSingleAccount)                                                      \
                                                                               \
  F(kPreSelectAccount)                                                         \
                                                                               \
  /* TODO(crbug.com/40284700): Merge with kSelectPriorityMechanism. */         \
  F(kPreSelectSingleAccount)                                                   \
                                                                               \
  /* kSelectPriorityMechanism lets the user confirm a single "priority" */     \
  /* mechanism. */                                                             \
  F(kSelectPriorityMechanism)                                                  \
                                                                               \
  /* Attestation permission requests. */                                       \
  F(kAttestationPermissionRequest)                                             \
  F(kEnterpriseAttestationPermissionRequest)                                   \
                                                                               \
  /* GPM Pin (6-digit). */                                                     \
  F(kGPMChangePin)                                                             \
  F(kGPMCreatePin)                                                             \
  F(kGPMEnterPin)                                                              \
                                                                               \
  /* GPM Pin (alphanumeric). */                                                \
  F(kGPMChangeArbitraryPin)                                                    \
  F(kGPMCreateArbitraryPin)                                                    \
  F(kGPMEnterArbitraryPin)                                                     \
                                                                               \
  /* User verification prompt for GPM. Only valid on macOS 12+. */             \
  F(kGPMTouchID)                                                               \
                                                                               \
  /* GPM passkey creation. */                                                  \
  F(kGPMCreatePasskey)                                                         \
  F(kGPMConfirmOffTheRecordCreate)                                             \
  F(kCreatePasskey)                                                            \
  F(kGPMError)                                                                 \
  F(kGPMConnecting)                                                            \
                                                                               \
  /* Device bootstrap to use GPM passkeys. */                                  \
  F(kRecoverSecurityDomain)                                                    \
  F(kTrustThisComputerAssertion)                                               \
  F(kTrustThisComputerCreation)                                                \
                                                                               \
  /* Changing GPM PIN. */                                                      \
  F(kGPMReauthForPinReset)                                                     \
  F(kGPMLockedPin)

// AuthenticatorRequestDialogModel holds the UI state for a WebAuthn request.
// This class is refcounted so that its ownership can be shared between the
// dialog view and the request delegate, which both depend on its state, and
// don't have coupled lifetimes.
struct AuthenticatorRequestDialogModel
    : public base::RefCounted<AuthenticatorRequestDialogModel> {
  enum class Step {
#define F(x) x,
    STEPS
#undef F
  };

  // Views and controllers implement this interface to receive events, which
  // flow both from controllers to the views, and from the views to the
  // controllers.
  class Observer : public base::CheckedObserver {
   public:
#define AUTHENTICATOR_REQUEST_EVENT_0(name) virtual void name();
#define AUTHENTICATOR_REQUEST_EVENT_1(name, arg1type) \
  virtual void name(arg1type arg1);
    AUTHENTICATOR_EVENTS
#undef AUTHENTICATOR_REQUEST_EVENT_0
#undef AUTHENTICATOR_REQUEST_EVENT_1
  };

  // A Mechanism is a user-visible method of authenticating. It might be a
  // transport (such as USB), a platform authenticator, a phone, or even a
  // delegation to a platform API. Selecting a mechanism starts the flow for the
  // user to authenticate with it (e.g. by showing a QR code or dispatching to a
  // platform authenticator).
  //
  // On get assertion requests, mechanisms can also represent credentials for
  // authenticators that support silent discovery. In this case, the |type| is
  // |Credential| and it is annotated with the source of the credential (phone,
  // icloud, etc). Selecting such a mechanism dispatches a request narrowed down
  // to the specific credential to an authenticator that can fulfill it.
  struct Mechanism {
    // These types describe the type of Mechanism.
    struct CredentialInfo {
      CredentialInfo(device::AuthenticatorType source_in,
                     std::vector<uint8_t> user_id_in);
      CredentialInfo(const CredentialInfo&);
      ~CredentialInfo();
      bool operator==(const CredentialInfo&) const;

      const device::AuthenticatorType source;
      const std::vector<uint8_t> user_id;
    };
    using Credential = base::StrongAlias<class CredentialTag, CredentialInfo>;
    using Transport =
        base::StrongAlias<class TransportTag, AuthenticatorTransport>;
    using WindowsAPI = base::StrongAlias<class WindowsAPITag, absl::monostate>;
    using ICloudKeychain =
        base::StrongAlias<class iCloudKeychainTag, absl::monostate>;
    using Phone = base::StrongAlias<class PhoneTag, std::string>;
    using AddPhone = base::StrongAlias<class AddPhoneTag, absl::monostate>;
    using Enclave = base::StrongAlias<class EnclaveTag, absl::monostate>;
    using SignInAgain =
        base::StrongAlias<class SignInAgainTag, absl::monostate>;
    using Type = absl::variant<Credential,
                               Transport,
                               WindowsAPI,
                               Phone,
                               AddPhone,
                               ICloudKeychain,
                               Enclave,
                               SignInAgain>;

    Mechanism(Type type,
              std::u16string name,
              std::u16string short_name,
              const gfx::VectorIcon& icon,
              base::RepeatingClosure callback);
    ~Mechanism();
    Mechanism(Mechanism&&);
    Mechanism(const Mechanism&) = delete;
    Mechanism& operator=(const Mechanism&) = delete;

    const Type type;
    const std::u16string name;
    const std::u16string short_name;
    std::u16string description;
    const raw_ref<const gfx::VectorIcon> icon;
    const base::RepeatingClosure callback;
  };

  // CableUIType enumerates the different types of caBLE UI that we've ended
  // up with.
  enum class CableUIType {
    CABLE_V1,
    CABLE_V2_SERVER_LINK,
    CABLE_V2_2ND_FACTOR,
  };

  explicit AuthenticatorRequestDialogModel(
      content::RenderFrameHost* render_frame_host);
  AuthenticatorRequestDialogModel(AuthenticatorRequestDialogModel&) = delete;
  AuthenticatorRequestDialogModel(const AuthenticatorRequestDialogModel&&) =
      delete;
  AuthenticatorRequestDialogModel& operator=(
      AuthenticatorRequestDialogModel&&) = delete;
  AuthenticatorRequestDialogModel& operator=(
      const AuthenticatorRequestDialogModel&) = delete;

  // This causes the events to become methods on the Model. Views and
  // Controllers call these methods to broadcast events to all observers.
#define AUTHENTICATOR_REQUEST_EVENT_0(name) void name();
#define AUTHENTICATOR_REQUEST_EVENT_1(name, arg1type) void name(arg1type arg1);
  AUTHENTICATOR_EVENTS
#undef AUTHENTICATOR_REQUEST_EVENT_0
#undef AUTHENTICATOR_REQUEST_EVENT_1

  // Below methods are used for base::ScopedObserver:
  void AddObserver(AuthenticatorRequestDialogModel::Observer* observer);
  void RemoveObserver(AuthenticatorRequestDialogModel::Observer* observer);

  // Views and controllers add themselves as observers here to receive events.
  base::ObserverList<Observer> observers;

  // The primary state of the model is the current `Step`. It's important that
  // this always be changed via `SetStep` so the field isn't exposed directly.
  Step step() const { return step_; }
  void SetStep(Step step);

  void DisableUiOrShowLoadingDialog();

  // generation is incremented each time the request is restarted so that events
  // from different request generations can be distinguished.
  int generation = 0;

  // The following methods and fields are read by views and both read and
  // written by controllers. Views use these values to determine what
  // information to show.

  // Returns whether the visible dialog should be closed. This means
  // that either the request has finished, or that the current step
  // has no UI, or a different style of UI.
  bool should_dialog_be_closed() const;

  device::FidoRequestType request_type = device::FidoRequestType::kGetAssertion;
  device::ResidentKeyRequirement resident_key_requirement =
      device::ResidentKeyRequirement::kDiscouraged;
  std::string relying_party_id;
  // mechanisms contains the entries that appear in the "transport" selection
  // sheet and the drop-down menu.
  std::vector<Mechanism> mechanisms;
  std::optional<size_t> priority_mechanism_index;
  // For MakeCredential requests, the PublicKeyCredentialUserEntity associated
  // with the request.
  device::PublicKeyCredentialUserEntity user_entity;
  // creds contains possible credentials to select between before or after an
  // authenticator has responded to a request.
  std::vector<device::DiscoverableCredentialMetadata> creds;
  // preselected_cred contains a credential preselected by the user.
  std::optional<device::DiscoverableCredentialMetadata> preselected_cred;
  // Whether the platform can check biometrics and has biometrics configured.
  std::optional<bool> platform_has_biometrics;
  // offer_try_again_in_ui indicates whether a button to retry the request
  // should be included on the dialog sheet shown when encountering certain
  // errors.
  bool offer_try_again_in_ui = true;
  bool ble_adapter_is_powered = false;
  // security_key_is_possible is true if a security key might be used for the
  // current transaction.
  bool security_key_is_possible = false;
  bool is_off_the_record = false;

  std::optional<int> max_bio_samples;
  std::optional<int> bio_samples_remaining;
  uint32_t min_pin_length = device::kMinPinLength;
  std::optional<int> pin_attempts;
  std::optional<int> uv_attempts;
  device::pin::PINEntryError pin_error = device::pin::PINEntryError::kNoError;
  // A sorted, unique list of the names of paired phones.
  std::vector<std::string> paired_phone_names;
  // The name of the priority phone, if any.
  std::optional<std::string> priority_phone_name;

  // cable_ui_type contains the type of UI to display for a caBLE transaction.
  std::optional<CableUIType> cable_ui_type;
  // cable_should_suggest_usb is true if the caBLE "v1" UI was triggered by
  // a caBLEv2 server-linked request and attaching a USB cable is an option.
  bool cable_should_suggest_usb = false;
  std::optional<std::string> cable_qr_string;
  // The name of the paired phone that was passed to `ContactPhone()`. It is
  // shown on the UI sheet that prompts the user to check their phone for
  // a notification.
  std::optional<std::string> selected_phone_name;

  // Number of remaining GPM pin entry attempts before getting locked out or
  // `std::nullopt` if there was no failed attempts during that request.
  std::optional<int> gpm_pin_remaining_attempts_;

  // Whether the UI is currently in a disabled state, which is required for some
  // transitions (e.g. `Step::kWaitingForEnclave`). Each step UI that needs it
  // should handle it accordingly.
  // TODO(rgod): Double check all steps and disable the needed elements.
  bool ui_disabled_ = false;

#if BUILDFLAG(IS_MAC)
  // lacontext contains an authenticated LAContext after a successful Touch ID
  // prompt.
  std::optional<crypto::ScopedLAContext> lacontext;
#endif  // BUILDFLAG(IS_MAC)

  // Returns the AccountInfo for the profile associated with the request.
  std::optional<AccountInfo> GetGpmAccountInfo();

  // Returns the account email for the profile associated with the request,
  // which may be empty.
  std::string GetGpmAccountEmail();

 private:
  friend class base::RefCounted<AuthenticatorRequestDialogModel>;
  ~AuthenticatorRequestDialogModel();

  // The Profile for the request, which may be an off-the-record profile.
  //
  // Returns nullptr if the RenderFrameHost for the request doesn't exist
  // anymore.
  Profile* GetProfile();

  Step step_ = Step::kNotStarted;
  const std::optional<content::GlobalRenderFrameHostId> frame_host_id;
};

std::ostream& operator<<(std::ostream& os,
                         const AuthenticatorRequestDialogModel::Step& step);

// Encapsulates the logic behind the WebAuthn UI flow.
// flow. This is essentially a state machine going through the states defined in
// the `Step` enumeration.
class AuthenticatorRequestDialogController
    : public AuthenticatorRequestDialogModel::Observer,
      public webauthn::PasskeyModel::Observer {
 public:
  using Model = AuthenticatorRequestDialogModel;
  using Step = AuthenticatorRequestDialogModel::Step;
  using Mechanism = AuthenticatorRequestDialogModel::Mechanism;
  using RequestCallback = device::FidoRequestHandlerBase::RequestCallback;
  using TransportAvailabilityInfo =
      device::FidoRequestHandlerBase::TransportAvailabilityInfo;
  using BlePermissionCallback = base::RepeatingCallback<void(
      device::FidoRequestHandlerBase::BlePermissionCallback)>;

  AuthenticatorRequestDialogController(
      Model* model,
      content::RenderFrameHost* render_frame_host);

  AuthenticatorRequestDialogController(
      const AuthenticatorRequestDialogController&) = delete;
  AuthenticatorRequestDialogController& operator=(
      const AuthenticatorRequestDialogController&) = delete;

  ~AuthenticatorRequestDialogController() override;

  Model* model() const;

  void OnModelDestroyed(AuthenticatorRequestDialogModel* model) override;

  // Hides the dialog. A subsequent call to SetCurrentStep() will unhide it.
  void HideDialog();

  // Returns whether the UI is in a state at which the |request_| member of
  // AuthenticatorImpl has completed processing. Note that the request callback
  // is only resolved after the UI is dismissed.
  bool is_request_complete() const {
    return model_->step() == Step::kTimedOut ||
           model_->step() == Step::kKeyNotRegistered ||
           model_->step() == Step::kKeyAlreadyRegistered ||
           model_->step() == Step::kMissingCapability ||
           model_->step() == Step::kErrorWindowsHelloNotEnabled ||
           model_->step() == Step::kClosed;
  }

  const TransportAvailabilityInfo* transport_availability() const {
    return &transport_availability_;
  }

  const std::optional<std::string>& selected_authenticator_id() const {
    return ephemeral_state_.selected_authenticator_id_;
  }

  // Starts the UX flow, by either showing the transport selection screen or
  // the guided flow for them most likely transport.
  //
  // If |is_conditional_mediation| is true, credentials will be shown on the
  // password autofill instead of the full-blown page-modal UI.
  //
  // Valid action when at step: kNotStarted.
  void StartFlow(TransportAvailabilityInfo trasport_availability,
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
      TransportAvailabilityInfo transport_availability);

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
  void ShowCableUsbFallback() override;

  // Show caBLE activation sheet.
  void ShowCable();

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
  void OnAttestationPermissionResponse(
      bool attestation_permission_granted) override;

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
  void SetCurrentStepForTesting(Step step);

  TransportAvailabilityInfo& transport_availability_for_testing() {
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

  void RequestAttestationPermission(bool is_enterprise_attestation,
                                    base::OnceCallback<void(bool)> callback);

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

    // Represents the id of the Bluetooth authenticator that the user is trying
    // to connect to or conduct WebAuthN request to via the WebAuthN UI.
    std::optional<std::string> selected_authenticator_id_;

    // Stores a list of |AuthenticatorReference| values such that a request can
    // be dispatched dispatched after some UI interaction. This is useful for
    // platform authenticators (and Windows) where dispatch to the authenticator
    // immediately results in modal UI to appear.
    ObservableAuthenticatorList saved_authenticators_;

    // responses_ contains possible responses to select between after an
    // authenticator has responded to a request.
    std::vector<device::AuthenticatorGetAssertionResponse> responses_;

    // did_dispatch_to_icloud_keychain_ is true if iCloud Keychain has been
    // triggered.
    bool did_dispatch_to_icloud_keychain_ = false;

    // did_invoke_platform_despite_no_priority_mechanism_ is true if a platform
    // authenticator was triggered despite there not being a
    // `priority_mechanism_index_` set. For example, this can happen if there's
    // an allowlist match.
    bool did_invoke_platform_despite_no_priority_mechanism_ = false;
  };

  void ResetEphemeralState();

  void SetCurrentStep(Step);

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

  raw_ptr<Model> model_;

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
  std::optional<Step> pending_step_;

  // after_off_the_record_interstitial_ contains the closure to run if the user
  // accepts the interstitial that warns that platform/caBLE authenticators may
  // record information even in incognito mode.
  base::OnceClosure after_off_the_record_interstitial_;

  // after_ble_adapter_powered_ contains the closure to run if the user
  // accepts the interstitial that requests to turn on the BLE adapter.
  base::OnceClosure after_ble_adapter_powered_;

  // This field is only filled out once the UX flow is started.
  TransportAvailabilityInfo transport_availability_;

  content::AuthenticatorRequestClientDelegate::AccountPreselectedCallback
      account_preselected_callback_;
  RequestCallback request_callback_;
  base::RepeatingClosure bluetooth_adapter_power_on_callback_;

  // Triggers a permission prompt on macOS if ble_status is
  // kPendingPermissionRequest and returns the bluetooth status.
  BlePermissionCallback request_ble_permission_callback_;

  base::OnceClosure bio_enrollment_callback_;

  base::OnceCallback<void(std::u16string)> pin_callback_;

  base::OnceCallback<void(bool)> attestation_callback_;

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

  const content::GlobalRenderFrameHostId frame_host_id_;

  base::ScopedObservation<webauthn::PasskeyModel,
                          webauthn::PasskeyModel::Observer>
      passkey_model_observation_{this};

  base::WeakPtrFactory<AuthenticatorRequestDialogController> weak_factory_{
      this};
};

#undef STEP

#endif  // CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_MODEL_H_
