// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_MODEL_H_
#define CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_MODEL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/types/strong_alias.h"
#include "build/build_config.h"
#include "chrome/browser/webauthn/authenticator_transport.h"
#include "chrome/browser/webauthn/observable_authenticator_list.h"
#include "content/public/browser/global_routing_id.h"
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

namespace gfx {
struct VectorIcon;
}

struct AccountInfo;
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

// AuthenticatorRequestDialogModel holds the UI state for a WebAuthn request.
// This class is refcounted so that its ownership can be shared between the
// dialog view and the request delegate, which both depend on its state, and
// don't have coupled lifetimes.
struct AuthenticatorRequestDialogModel
    : public base::RefCounted<AuthenticatorRequestDialogModel> {
  // Each Step defines a unique UI state. Setting a Step causes the matching
  // dialog or window to appear.
  //
  // Append new values to the end and update `kMaxValue`.
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
    kErrorWindowsHelloNotEnabled,
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
    kBlePermissionMac,
    // Let the user confirm that they want to create a credential in an
    // off-the-record browsing context. Used for platform and caBLE
    // credential, where we feel that it's perhaps not obvious that something
    // will be recorded.
    kOffTheRecordInterstitial,
    // Phone as a security key.
    kPhoneConfirmationSheet,
    kCableActivate,
    kCableV2QRCode,
    kCableV2Connecting,
    kCableV2Connected,
    kCableV2Error,
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
    // Account selection. This occurs prior to performing user verification
    // for platform authenticators ("pre-select"), or afterwards for USB
    // security keys. In each mode, there are different sheets for confirming
    // a single available credential and choosing one from a list of multiple
    // options.
    kSelectAccount,
    kSelectSingleAccount,
    kPreSelectAccount,
    // TODO(crbug.com/40284700): Merge with kSelectPriorityMechanism.
    kPreSelectSingleAccount,
    // kSelectPriorityMechanism lets the user confirm a single "priority"
    // mechanism.
    kSelectPriorityMechanism,
    // GPM Pin (6-digit).
    kGPMChangePin,
    kGPMCreatePin,
    kGPMEnterPin,
    // GPM Pin (alphanumeric).
    kGPMChangeArbitraryPin,
    kGPMCreateArbitraryPin,
    kGPMEnterArbitraryPin,
    // User verification prompt for GPM. Only valid on macOS 12+.
    kGPMTouchID,
    // GPM passkey creation.
    kGPMCreatePasskey,
    kGPMConfirmOffTheRecordCreate,
    kCreatePasskey,
    kGPMError,
    kGPMConnecting,
    // Device bootstrap to use GPM passkeys.
    kRecoverSecurityDomain,
    kTrustThisComputerAssertion,
    kTrustThisComputerCreation,
    // Changing GPM PIN.
    kGPMReauthForPinReset,
    kGPMLockedPin,
    kMaxValue = kGPMLockedPin,
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
  // This value is present if, and only if, `request_type` is `kMakeCredential`.
  std::optional<device::AttestationConveyancePreference>
      attestation_conveyance_preference;
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
  // show_security_key_on_qr_sheet is true if the security key option should be
  // offered on the QR sheet.
  bool show_security_key_on_qr_sheet = false;
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

  std::optional<std::string> cable_qr_string;
  // The name of the paired phone that was passed to `ContactPhone()`. It is
  // shown on the UI sheet that prompts the user to check their phone for
  // a notification.
  std::optional<std::string> selected_phone_name;

  // Number of remaining GPM pin entry attempts before getting locked out or
  // `std::nullopt` if there was no failed attempts during that request.
  std::optional<int> gpm_pin_remaining_attempts_;

  // Whether the UI is currently in a disabled state, which is required for some
  // transitions (e.g. when waiting for the response from the enclave). When
  // true, the sheets will by default display an activity indicator at the top
  // and disable all the usual buttons (e.g. accept or "other mechanisms")
  // except for the cancel button.
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

#endif  // CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_MODEL_H_
