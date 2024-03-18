// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_CHROME_AUTHENTICATOR_REQUEST_DELEGATE_H_
#define CHROME_BROWSER_WEBAUTHN_CHROME_AUTHENTICATOR_REQUEST_DELEGATE_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/global_routing_id.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "google_apis/gaia/google_service_auth_error.h"

class EnclaveManager;
class PrefService;
class Profile;

namespace content {
class BrowserContext;
class RenderFrameHost;
}  // namespace content

namespace device {
class FidoAuthenticator;
class FidoDiscoveryFactory;
class PublicKeyCredentialDescriptor;
class PublicKeyCredentialUserEntity;
enum class FidoRequestType : uint8_t;
namespace enclave {
struct ClaimedPIN;
struct CredentialRequest;
}
}  // namespace device

namespace signin {
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

namespace sync_pb {
class WebauthnCredentialSpecifics;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// ChromeWebAuthenticationDelegate is the //chrome layer implementation of
// content::WebAuthenticationDelegate.
class ChromeWebAuthenticationDelegate
    : public content::WebAuthenticationDelegate {
 public:
#if BUILDFLAG(IS_MAC)
  // Returns a configuration struct for instantiating the macOS WebAuthn
  // platform authenticator for the given Profile.
  static TouchIdAuthenticatorConfig TouchIdAuthenticatorConfigForProfile(
      Profile* profile);
#endif  // BUILDFLAG(IS_MAC)

  ~ChromeWebAuthenticationDelegate() override;

  // content::WebAuthenticationDelegate:
  bool OverrideCallerOriginAndRelyingPartyIdValidation(
      content::BrowserContext* browser_context,
      const url::Origin& caller_origin,
      const std::string& relying_party_id) override;
  bool OriginMayUseRemoteDesktopClientOverride(
      content::BrowserContext* browser_context,
      const url::Origin& caller_origin) override;
  std::optional<std::string> MaybeGetRelyingPartyIdOverride(
      const std::string& claimed_relying_party_id,
      const url::Origin& caller_origin) override;
  bool ShouldPermitIndividualAttestation(
      content::BrowserContext* browser_context,
      const url::Origin& caller_origin,
      const std::string& relying_party_id) override;
  bool SupportsResidentKeys(
      content::RenderFrameHost* render_frame_host) override;
  bool SupportsPasskeyMetadataSyncing() override;
  bool IsFocused(content::WebContents* web_contents) override;
  std::optional<bool> IsUserVerifyingPlatformAuthenticatorAvailableOverride(
      content::RenderFrameHost* render_frame_host) override;
  content::WebAuthenticationRequestProxy* MaybeGetRequestProxy(
      content::BrowserContext* browser_context,
      const url::Origin& caller_origin) override;
  bool IsEnclaveAuthenticatorAvailable(
      content::BrowserContext* browser_context) override;

#if BUILDFLAG(IS_MAC)
  std::optional<TouchIdAuthenticatorConfig> GetTouchIdAuthenticatorConfig(
      content::BrowserContext* browser_context) override;
#endif  // BUILDFLAG(IS_MAC)
#if BUILDFLAG(IS_CHROMEOS)
  ChromeOSGenerateRequestIdCallback GetGenerateRequestIdCallback(
      content::RenderFrameHost* render_frame_host) override;
#endif  // BUILDFLAG(IS_CHROMEOS)
};

class ChromeAuthenticatorRequestDelegate
    : public content::AuthenticatorRequestClientDelegate,
      public AuthenticatorRequestDialogModel::Observer {
 public:
  // TestObserver is an interface that observes certain events related to this
  // class for testing purposes. Only a single instance of this interface can
  // be installed at a given time.
  class TestObserver {
   public:
    virtual void Created(ChromeAuthenticatorRequestDelegate* delegate) = 0;

    virtual std::vector<std::unique_ptr<device::cablev2::Pairing>>
    GetCablePairingsFromSyncedDevices() = 0;

    virtual void OnTransportAvailabilityEnumerated(
        ChromeAuthenticatorRequestDelegate* delegate,
        device::FidoRequestHandlerBase::TransportAvailabilityInfo* tai) = 0;

    virtual void UIShown(ChromeAuthenticatorRequestDelegate* delegate) = 0;

    virtual void CableV2ExtensionSeen(
        base::span<const uint8_t> server_link_data) = 0;

    virtual void ConfiguringCable(device::FidoRequestType request_type) {}

    virtual void AccountSelectorShown(
        const std::vector<device::AuthenticatorGetAssertionResponse>&
            responses) {}
  };

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
  // The |render_frame_host| must outlive this instance.
  explicit ChromeAuthenticatorRequestDelegate(
      content::RenderFrameHost* render_frame_host);

  ChromeAuthenticatorRequestDelegate(
      const ChromeAuthenticatorRequestDelegate&) = delete;
  ChromeAuthenticatorRequestDelegate& operator=(
      const ChromeAuthenticatorRequestDelegate&) = delete;

  ~ChromeAuthenticatorRequestDelegate() override;

  // SetGlobalObserverForTesting sets the single |TestObserver| that is active
  // at a given time. Call be called with |nullptr| to unregister a
  // |TestObserver|. It is a fatal error to try and register a |TestObserver|
  // while one is still installed.
  static void SetGlobalObserverForTesting(TestObserver*);

  base::WeakPtr<ChromeAuthenticatorRequestDelegate> AsWeakPtr();

  AuthenticatorRequestDialogModel* dialog_model() const {
    return dialog_model_.get();
  }

  // content::AuthenticatorRequestClientDelegate:
  void SetRelyingPartyId(const std::string& rp_id) override;
  bool DoesBlockRequestOnFailure(InterestingFailureReason reason) override;
  void RegisterActionCallbacks(
      base::OnceClosure cancel_callback,
      base::RepeatingClosure start_over_callback,
      AccountPreselectedCallback account_preselected_callback,
      device::FidoRequestHandlerBase::RequestCallback request_callback,
      base::RepeatingClosure bluetooth_adapter_power_on_callback) override;
  void OnTransactionSuccessful(RequestSource request_source,
                               device::FidoRequestType,
                               device::AuthenticatorType) override;
  void ShouldReturnAttestation(
      const std::string& relying_party_id,
      const device::FidoAuthenticator* authenticator,
      bool is_enterprise_attestation,
      base::OnceCallback<void(bool)> callback) override;
  void ConfigureDiscoveries(
      const url::Origin& origin,
      const std::string& rp_id,
      RequestSource request_source,
      device::FidoRequestType request_type,
      std::optional<device::ResidentKeyRequirement> resident_key_requirement,
      device::UserVerificationRequirement user_verification_requirement,
      base::span<const device::CableDiscoveryData> pairings_from_extension,
      bool is_enclave_authenticator_available,
      device::FidoDiscoveryFactory* discovery_factory) override;
  void SetHints(
      const AuthenticatorRequestClientDelegate::Hints& hints) override;
  void SelectAccount(
      std::vector<device::AuthenticatorGetAssertionResponse> responses,
      base::OnceCallback<void(device::AuthenticatorGetAssertionResponse)>
          callback) override;
  void DisableUI() override;
  bool IsWebAuthnUIEnabled() override;
  void SetConditionalRequest(bool is_conditional) override;
  void SetCredentialIdFilter(std::vector<device::PublicKeyCredentialDescriptor>
                                 credential_list) override;
  void SetUserEntityForMakeCredentialRequest(
      const device::PublicKeyCredentialUserEntity& user_entity) override;

  // device::FidoRequestHandlerBase::Observer:
  void OnTransportAvailabilityEnumerated(
      device::FidoRequestHandlerBase::TransportAvailabilityInfo data) override;
  bool EmbedderControlsAuthenticatorDispatch(
      const device::FidoAuthenticator& authenticator) override;
  void FidoAuthenticatorAdded(
      const device::FidoAuthenticator& authenticator) override;
  void FidoAuthenticatorRemoved(base::StringPiece authenticator_id) override;
  void BluetoothAdapterPowerChanged(bool is_powered_on) override;
  bool SupportsPIN() const override;
  void CollectPIN(
      CollectPINOptions options,
      base::OnceCallback<void(std::u16string)> provide_pin_cb) override;
  void StartBioEnrollment(base::OnceClosure next_callback) override;
  void OnSampleCollected(int bio_samples_remaining) override;
  void FinishCollectToken() override;
  void OnRetryUserVerification(int attempts) override;

  // AuthenticatorRequestDialogModel::Observer:
  void OnStartOver() override;
  void OnModelDestroyed(AuthenticatorRequestDialogModel* model) override;
  void OnStepTransition() override;
  void OnCancelRequest() override;
  void OnManageDevicesClicked() override;

  // A non-const version of dialog_model().
  AuthenticatorRequestDialogModel* GetDialogModelForTesting();

  // SetPassEmptyUsbDeviceManagerForTesting controls whether the
  // `DiscoveryFactory` will be given an empty USB device manager. This is
  // needed in tests because creating a real `device::mojom::UsbDeviceManager`
  // can create objects on thread-pool threads. Those objects aren't scheduled
  // for deletion until after the thread-pool is shutdown when testing, causing
  // "leaks" to be reported.
  void SetPassEmptyUsbDeviceManagerForTesting(bool value);

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromeAuthenticatorRequestDelegatePrivateTest,
                           DaysSinceDate);
  FRIEND_TEST_ALL_PREFIXES(ChromeAuthenticatorRequestDelegatePrivateTest,
                           GetICloudKeychainPref);
  FRIEND_TEST_ALL_PREFIXES(ChromeAuthenticatorRequestDelegatePrivateTest,
                           ShouldCreateInICloudKeychain);

  class EnclaveManagerObserver;

  // GetRenderFrameHost returns a pointer to the RenderFrameHost that was given
  // to the constructor.
  content::RenderFrameHost* GetRenderFrameHost() const;

  content::BrowserContext* GetBrowserContext() const;

  void ShowUI(device::FidoRequestHandlerBase::TransportAvailabilityInfo data);

  std::optional<device::FidoTransportProtocol> GetLastTransportUsed() const;

  // Called when the EnclaveManager has finished loading its state from the
  // disk.
  void OnEnclaveLoaded();

  // Called when MagicArch has provided keys to the EnclaveManager.
  void OnKeysStored();

  // Called when the current device has been added to the security domain.
  void OnDeviceAdded(bool success);

  // Called when the user selects an account from modal or conditional UI.
  // Stores the credential ID in `preselected_cred_id_` then forwards to the
  // `AccountPreselectedCallback` that was passed to
  // `RegisterActionCallbacks`.
  void OnAccountPreselected(device::DiscoverableCredentialMetadata);

  // Called to start fetching the state of the primary account from the
  // trusted vault service.
  void DownloadAccountState();

  // Tell `dialog_model_` that the enclave manager is ready.
  void SetAccountStateReady();

  // Called when the state of the trusted vault has been determined by
  // `DownloadAccountState`.
  void OnAccountStateDownloaded(
      std::unique_ptr<trusted_vault::TrustedVaultConnection> unused,
      trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
          result);

  // Called when the UI has reached a state where it needs to do an enclave
  // operation, and an OAuth token for the enclave has been fetched.
  void MaybeHashPinAndStartEnclaveTransaction(std::optional<std::string> token);

  // Called when the UI has reached a state where it needs to do an enclave
  // operation, an OAuth token for the enclave has been fetched, and any PIN
  // hashing has been completed.
  void StartEnclaveTransaction(std::optional<std::string> token,
                               std::unique_ptr<device::enclave::ClaimedPIN>);

  // ShouldPermitCableExtension returns true if the given |origin| may set a
  // caBLE extension. This extension contains website-chosen BLE pairing
  // information that will be broadcast by the device.
  bool ShouldPermitCableExtension(const url::Origin& origin);

  void OnInvalidatedCablePairing(
      std::unique_ptr<device::cablev2::Pairing> failed_pairing);
  void OnCableEvent(device::cablev2::Event event);

  // Adds GPM passkeys matching |rp_id| to |passkeys|.
  void GetPhoneContactableGpmPasskeysForRpId(
      std::vector<device::DiscoverableCredentialMetadata>* passkeys);

  // Configures an WebAuthn enclave authenticator discovery and provides it with
  // synced passkeys.
  void ConfigureEnclaveDiscovery(
      const std::string& rp_id,
      device::FidoDiscoveryFactory* discovery_factory);

  // Invoked when a new GPM passkey is created, to save it to sync data.
  void OnPasskeyCreated(sync_pb::WebauthnCredentialSpecifics passkey);

#if BUILDFLAG(IS_MAC)
  // DaysSinceDate returns the number of days between `formatted_date` (in ISO
  // 8601 format) and `now`. It returns `nullopt` if `formatted_date` cannot be
  // parsed or if it's in `now`s future.
  //
  // It does not parse `formatted_date` strictly and is intended for trusted
  // inputs.
  static std::optional<int> DaysSinceDate(const std::string& formatted_date,
                                          base::Time now);

  // GetICloudKeychainPref returns the value of the iCloud Keychain preference
  // as a tristate. If no value for the preference has been set then it
  // returns `std::nullopt`.
  static std::optional<bool> GetICloudKeychainPref(const PrefService* prefs);

  // IsActiveProfileAuthenticatorUser returns true if the profile authenticator
  // has been used in the past 31 days.
  static bool IsActiveProfileAuthenticatorUser(const PrefService* prefs);

  // ShouldCreateInICloudKeychain returns true if attachment=platform creation
  // requests should default to iCloud Keychain.
  static bool ShouldCreateInICloudKeychain(
      RequestSource request_source,
      bool is_active_profile_authenticator_user,
      bool has_icloud_drive_enabled,
      bool request_is_for_google_com,
      std::optional<bool> preference);

  // ConfigureICloudKeychain is called by `ConfigureDiscoveries` to configure
  // the `AuthenticatorRequestDialogModel` with iCloud Keychain-related values.
  void ConfigureICloudKeychain(RequestSource request_source,
                               const std::string& rp_id);
#endif

  const content::GlobalRenderFrameHostId render_frame_host_id_;
  const std::unique_ptr<AuthenticatorRequestDialogModel> dialog_model_;
  base::OnceClosure cancel_callback_;
  base::RepeatingClosure start_over_callback_;
  AccountPreselectedCallback account_preselected_callback_;
  device::FidoRequestHandlerBase::RequestCallback request_callback_;

  // If in the TransportAvailabilityInfo reported by the request handler,
  // disable_embedder_ui is set, this will be set to true. No UI must be
  // rendered and all request handler callbacks will be ignored.
  bool disable_ui_ = false;

  // If true, show a more subtle UI unless the user has platform discoverable
  // credentials on the device.
  bool is_conditional_ = false;

  // A list of credentials used to filter passkeys by ID. When non-empty,
  // non-matching passkeys will not be displayed during conditional mediation
  // requests. When empty, no filter is applied and all passkeys are displayed.
  std::vector<device::PublicKeyCredentialDescriptor> credential_filter_;

  // See `SetPassEmptyUsbDeviceManagerForTesting`.
  bool pass_empty_usb_device_manager_ = false;

  // cable_device_ready_ is true if a caBLE handshake has completed. At this
  // point we assume that any errors were communicated on the caBLE device and
  // don't show errors on the desktop too.
  bool cable_device_ready_ = false;

  // can_use_synced_phone_passkeys_ is true if there is a phone pairing
  // available that can service requests for synced GPM passkeys.
  bool can_use_synced_phone_passkeys_ = false;

  // Non-null when the cloud enclave authenticator is available for use. The
  // `EnclaveManager` is a `KeyedService` for the current profile and so
  // outlives this object.
  raw_ptr<EnclaveManager> enclave_manager_ = nullptr;

  // Used to observe `enclave_manager_` without implementing the Observer
  // interface directly and thus needing to #include all of `enclave_manager.h`
  // in this header file.
  std::unique_ptr<EnclaveManagerObserver> enclave_manager_observer_;

  // Stores the TransportAvailabilityInfo while we're waiting for the enclave
  // state to load from the disk.
  std::unique_ptr<device::FidoRequestHandlerBase::TransportAvailabilityInfo>
      pending_transport_availability_info_;

  std::optional<device::FidoRequestType> request_type_;

  std::optional<device::UserVerificationRequirement>
      user_verification_requirement_;

  // The set of pertinent synced passkeys for this request. Persisted here
  // so that a consistent set of passkeys is used throughout the transaction.
  std::optional<std::vector<sync_pb::WebauthnCredentialSpecifics>>
      gpm_credentials_;

  // The pending request to fetch the state of the trusted vault.
  std::unique_ptr<trusted_vault::TrustedVaultConnection::Request>
      download_account_state_request_;

  // The pending request to fetch an OAuth token for the enclave request.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;

  // The callback used to trigger a request to the enclave.
  base::RepeatingCallback<void(
      std::unique_ptr<device::enclave::CredentialRequest>)>
      enclave_request_callback_;

  // The credential ID of the last credential to be selected by the user in
  // modal or conditional UI.
  std::optional<std::vector<uint8_t>> preselected_cred_id_;

  // Contains the bytes of a WrappedPIN structure, downloaded from the security
  // domain service.
  std::optional<std::string> serialized_wrapped_pin_;

  // Hold the GPM PIN in the special case where we prompt for a PIN to add one
  // to the account, but then immediately need it in order to satisfy UV for
  // the request.
  std::optional<std::string> gpm_pin_stashed_;

  base::WeakPtrFactory<ChromeAuthenticatorRequestDelegate> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_WEBAUTHN_CHROME_AUTHENTICATOR_REQUEST_DELEGATE_H_
