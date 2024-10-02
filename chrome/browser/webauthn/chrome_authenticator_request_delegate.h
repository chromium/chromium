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
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
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
#include "third_party/blink/public/mojom/credentialmanagement/credential_type_flags.mojom.h"

class AuthenticatorRequestDialogController;
class GPMEnclaveController;
class PrefService;
class Profile;

namespace base {
class Clock;
}

namespace chromeos {
class PasskeyDialogController;
}

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
}  // namespace device

namespace user_prefs {
class PrefRegistrySyncable;
}

// ChromeWebAuthenticationDelegate is the //chrome layer implementation of
// content::WebAuthenticationDelegate.
class ChromeWebAuthenticationDelegate final
    : public content::WebAuthenticationDelegate {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class SignalUnknownCredentialResult {
    kPasskeyNotFound = 0,
    kPasskeyRemoved = 1,
    kMaxValue = kPasskeyRemoved,
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class SignalAllAcceptedCredentialsResult {
    kNoPasskeyRemoved = 0,
    kPasskeyRemoved = 1,
    kMaxValue = kPasskeyRemoved,
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class SignalCurrentUserDetailsResult {
    kQuotaExceeded = 0,
    kPasskeyUpdated = 1,
    kPasskeyNotUpdated = 2,
    kMaxValue = kPasskeyNotUpdated,
  };

#if BUILDFLAG(IS_MAC)
  // Returns a configuration struct for instantiating the macOS WebAuthn
  // platform authenticator for the given Profile.
  static TouchIdAuthenticatorConfig TouchIdAuthenticatorConfigForProfile(
      Profile* profile);
#endif  // BUILDFLAG(IS_MAC)

  ChromeWebAuthenticationDelegate();

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
  void IsUserVerifyingPlatformAuthenticatorAvailableOverride(
      content::RenderFrameHost* render_frame_host,
      base::OnceCallback<void(std::optional<bool>)> callback) override;
  content::WebAuthenticationRequestProxy* MaybeGetRequestProxy(
      content::BrowserContext* browser_context,
      const url::Origin& caller_origin) override;
  void DeletePasskey(content::WebContents* web_contents,
                     const std::vector<uint8_t>& passkey_credential_id,
                     const std::string& relying_party_id) override;
  void DeleteUnacceptedPasskeys(content::WebContents* web_contents,
                                const std::string& relying_party_id,
                                const std::vector<uint8_t>& user_id,
                                const std::vector<std::vector<uint8_t>>&
                                    all_accepted_credentials_ids) override;
  void UpdateUserPasskeys(content::WebContents* web_contents,
                          const url::Origin& origin,
                          const std::string& relying_party_id,
                          std::vector<uint8_t>& user_id,
                          const std::string& name,
                          const std::string& display_name) override;
  void BrowserProvidedPasskeysAvailable(
      content::BrowserContext* browser_context,
      base::OnceCallback<void(bool)> callback) override;

#if BUILDFLAG(IS_MAC)
  std::optional<TouchIdAuthenticatorConfig> GetTouchIdAuthenticatorConfig(
      content::BrowserContext* browser_context) override;
#endif  // BUILDFLAG(IS_MAC)
#if BUILDFLAG(IS_CHROMEOS)
  ChromeOSGenerateRequestIdCallback GetGenerateRequestIdCallback(
      content::RenderFrameHost* render_frame_host) override;
#endif  // BUILDFLAG(IS_CHROMEOS)

 private:
  // Caches the result from looking up whether a TPM is available for Enclave
  // requests.
  std::optional<bool> tpm_available_;
  base::WeakPtrFactory<ChromeWebAuthenticationDelegate> weak_ptr_factory_{this};
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
    virtual void Created(ChromeAuthenticatorRequestDelegate* delegate) {}

    virtual void OnDestroy(ChromeAuthenticatorRequestDelegate* delegate) {}

    virtual std::vector<std::unique_ptr<device::cablev2::Pairing>>
    GetCablePairingsFromSyncedDevices();

    virtual void OnTransportAvailabilityEnumerated(
        ChromeAuthenticatorRequestDelegate* delegate,
        device::FidoRequestHandlerBase::TransportAvailabilityInfo* tai) {}

    virtual void UIShown(ChromeAuthenticatorRequestDelegate* delegate) {}

    virtual void CableV2ExtensionSeen(
        base::span<const uint8_t> server_link_data) {}

    virtual void ConfiguringCable(device::FidoRequestType request_type) {}

    virtual void AccountSelectorShown(
        const std::vector<device::AuthenticatorGetAssertionResponse>&
            responses) {}

    virtual void HintsSet(
        const AuthenticatorRequestClientDelegate::Hints& hints) {}
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

  AuthenticatorRequestDialogController* dialog_controller() const {
    return dialog_controller_.get();
  }

  GPMEnclaveController* enclave_controller_for_testing() const;
#if BUILDFLAG(IS_CHROMEOS)
  chromeos::PasskeyDialogController& chromeos_passkey_controller_for_testing()
      const;
#endif

  // content::AuthenticatorRequestClientDelegate:
  void SetRelyingPartyId(const std::string& rp_id) override;
  bool DoesBlockRequestOnFailure(InterestingFailureReason reason) override;
  void RegisterActionCallbacks(
      base::OnceClosure cancel_callback,
      base::RepeatingClosure start_over_callback,
      AccountPreselectedCallback account_preselected_callback,
      device::FidoRequestHandlerBase::RequestCallback request_callback,
      base::RepeatingClosure bluetooth_adapter_power_on_callback,
      base::RepeatingCallback<
          void(device::FidoRequestHandlerBase::BlePermissionCallback)>
          bluetooth_query_status_callback) override;
  void OnTransactionSuccessful(RequestSource request_source,
                               device::FidoRequestType,
                               device::AuthenticatorType) override;
  void ConfigureDiscoveries(
      const url::Origin& origin,
      const std::string& rp_id,
      RequestSource request_source,
      device::FidoRequestType request_type,
      std::optional<device::ResidentKeyRequirement> resident_key_requirement,
      device::UserVerificationRequirement user_verification_requirement,
      std::optional<std::string_view> user_name,
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
  void SetAmbientCredentialTypes(int credential_type_flags) override;
  void SetCredentialIdFilter(std::vector<device::PublicKeyCredentialDescriptor>
                                 credential_list) override;
  void SetUserEntityForMakeCredentialRequest(
      const device::PublicKeyCredentialUserEntity& user_entity) override;
  std::vector<std::unique_ptr<device::FidoDiscoveryBase>>
  CreatePlatformDiscoveries() override;

  // device::FidoRequestHandlerBase::Observer:
  void OnTransportAvailabilityEnumerated(
      device::FidoRequestHandlerBase::TransportAvailabilityInfo data) override;
  bool EmbedderControlsAuthenticatorDispatch(
      const device::FidoAuthenticator& authenticator) override;
  void FidoAuthenticatorAdded(
      const device::FidoAuthenticator& authenticator) override;
  void FidoAuthenticatorRemoved(std::string_view authenticator_id) override;
  void BluetoothAdapterStatusChanged(
      device::FidoRequestHandlerBase::BleStatus ble_status) override;
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
  void OnCancelRequest() override;
  void OnManageDevicesClicked() override;

  // Allows setting a mock `TrustedVaultConnection` so a real one will not be
  // created. This is only used for a single request, and is destroyed
  // afterward.
  void SetTrustedVaultConnectionForTesting(
      std::unique_ptr<trusted_vault::TrustedVaultConnection> connection);

  void SetClockForTesting(base::Clock*);

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

  void OnReadyForUI() override;

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

  // Update `tai` to remove credentials that aren't applicable to this request.
  void FilterRecognizedCredentials(
      device::FidoRequestHandlerBase::TransportAvailabilityInfo* tai);

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

  // Configure the NSWindow* for the current RenderFrameHost. This is used by
  // some macOS system APIs to center dialogs on the pertinent Chrome window.
  void ConfigureNSWindow(device::FidoDiscoveryFactory* discovery_factory);

  // ConfigureICloudKeychain is called by `ConfigureDiscoveries` to configure
  // the `AuthenticatorRequestDialogController` with iCloud Keychain-related
  // values.
  void ConfigureICloudKeychain(RequestSource request_source,
                               const std::string& rp_id);
#endif

  const content::GlobalRenderFrameHostId render_frame_host_id_;
  const scoped_refptr<AuthenticatorRequestDialogModel> dialog_model_;
  const std::unique_ptr<AuthenticatorRequestDialogController>
      dialog_controller_;
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

  // The number of credential types that have been requested to be displayed
  // in the Ambient credential UI.
  int ambient_credential_types_ =
      static_cast<int>(blink::mojom::CredentialTypeFlags::kNone);

  // A list of credentials used to filter passkeys by ID. When non-empty,
  // non-matching passkeys will not be displayed during conditional mediation
  // requests. When empty, no filter is applied and all passkeys are displayed.
  std::vector<device::PublicKeyCredentialDescriptor> credential_filter_;


  // cable_device_ready_ is true if a caBLE handshake has completed. At this
  // point we assume that any errors were communicated on the caBLE device and
  // don't show errors on the desktop too.
  bool cable_device_ready_ = false;

  // can_use_synced_phone_passkeys_ is true if there is a phone pairing
  // available that can service requests for synced GPM passkeys.
  bool can_use_synced_phone_passkeys_ = false;

#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<chromeos::PasskeyDialogController>
      chromeos_passkey_controller_;
#endif

  // TODO(crbug.com/40187814): Don't define this on ChromeOS.
  std::unique_ptr<GPMEnclaveController> enclave_controller_;

  // Stores the TransportAvailabilityInfo while we're waiting for the enclave
  // state to load from the disk.
  std::unique_ptr<device::FidoRequestHandlerBase::TransportAvailabilityInfo>
      pending_transport_availability_info_;

  std::optional<device::FidoRequestType> request_type_;

  std::optional<device::UserVerificationRequirement>
      user_verification_requirement_;

  // This holds a `TrustedVaultConnection` which will be set on
  // `enclave_controller_` when it is created.
  std::unique_ptr<trusted_vault::TrustedVaultConnection>
      pending_trusted_vault_connection_;
  raw_ptr<base::Clock> clock_ = nullptr;

  base::WeakPtrFactory<ChromeAuthenticatorRequestDelegate> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_WEBAUTHN_CHROME_AUTHENTICATOR_REQUEST_DELEGATE_H_
