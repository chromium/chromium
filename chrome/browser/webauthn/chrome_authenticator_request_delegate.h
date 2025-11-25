// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_CHROME_AUTHENTICATOR_REQUEST_DELEGATE_H_
#define CHROME_BROWSER_WEBAUTHN_CHROME_AUTHENTICATOR_REQUEST_DELEGATE_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/password_credential_fetcher.h"
#include "chrome/browser/webauthn/password_credential_ui_controller.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/global_routing_id.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "third_party/blink/public/mojom/credentialmanagement/credential_type_flags.mojom.h"

class AuthenticatorRequestDialogController;
class GPMEnclaveController;
class PrefService;

namespace base {
class SequencedTaskRunner;
class TickClock;
}  // namespace base

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

    virtual void OnTransportAvailabilityEnumerated(
        ChromeAuthenticatorRequestDelegate* delegate,
        device::FidoRequestHandlerBase::TransportAvailabilityInfo* tai) {}

    // Called when TAI enumeration has finished but it might have to wait for
    // enclave availability.
    virtual void OnPreTransportAvailabilityEnumerated(
        ChromeAuthenticatorRequestDelegate* delegate) {}

    // Called when the UI dialog is shown.
    virtual void UIShown(ChromeAuthenticatorRequestDelegate* delegate) {}

    virtual void CableV2ExtensionSeen(
        base::span<const uint8_t> server_link_data) {}

    virtual void ConfiguringCable(device::FidoRequestType request_type) {}

    virtual void AccountSelectorShown(
        const std::vector<device::AuthenticatorGetAssertionResponse>&
            responses) {}

    virtual void HintsSet(
        const AuthenticatorRequestClientDelegate::Hints& hints) {}

    // Called right before `start_over_callback_` is invoked.
    virtual void PreStartOver() {}
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

  // content::AuthenticatorRequestClientDelegate:
  void SetRelyingPartyId(const std::string& rp_id) override;
  void SetUIPresentation(UIPresentation ui_presentation) override;
  bool DoesBlockRequestOnFailure(InterestingFailureReason reason) override;
  void RegisterActionCallbacks(
      base::OnceClosure cancel_callback,
      base::OnceClosure immediate_not_found_callback,
      base::RepeatingClosure start_over_callback,
      AccountPreselectedCallback account_preselected_callback,
      PasswordSelectedCallback password_selected_callback,
      device::FidoRequestHandlerBase::RequestCallback request_callback,
      base::OnceClosure cancel_ui_timeout_callback,
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
  void SetCredentialTypes(int credential_type_flags) override;
  void SetCredentialIdFilter(std::vector<device::PublicKeyCredentialDescriptor>
                                 credential_list) override;
  void SetUserEntityForMakeCredentialRequest(
      const device::PublicKeyCredentialUserEntity& user_entity) override;
  void ProvideChallengeUrl(
      const GURL& url,
      base::OnceCallback<void(std::optional<base::span<const uint8_t>>)>
          callback) override;

  // device::FidoRequestHandlerBase::Observer:
  void StartObserving(device::FidoRequestHandlerBase* request_handler) override;
  void StopObserving(device::FidoRequestHandlerBase* request_handler) override;
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

  void SetPasswordUIControllerForTesting(
      std::unique_ptr<PasswordCredentialUIController> controller);
  void SetPasswordFetcherForTesting(
      std::unique_ptr<PasswordCredentialFetcher> fetcher);

  // GetRenderFrameHost returns a pointer to the RenderFrameHost that was given
  // to the constructor.
  content::RenderFrameHost* GetRenderFrameHost() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromeAuthenticatorRequestDelegatePrivateTest,
                           DaysSinceDate);
  FRIEND_TEST_ALL_PREFIXES(ChromeAuthenticatorRequestDelegatePrivateTest,
                           GetICloudKeychainPref);
  FRIEND_TEST_ALL_PREFIXES(ChromeAuthenticatorRequestDelegatePrivateTest,
                           ShouldCreateInICloudKeychain);

  content::BrowserContext* GetBrowserContext() const;
  Profile* profile() const;

  bool webauthn_ui_enabled() const;

  // Returns `true` iff the handled request is an immediate `get()` request and
  // no immediately available credentials found. This will trigger the
  // `immediate_not_found_callback_` to notify the renderer.
  bool MaybeHandleImmediateMediation(
      const device::FidoRequestHandlerBase::TransportAvailabilityInfo& data,
      const PasswordCredentialFetcher::PasswordCredentials& passwords);

  // Barriers showing the UI while waiting for
  // - password credentials,
  // - WebAuthn credentials,
  // - enclave readiness.
  void TryToShowUI();

  void MaybeShowUI(
      device::FidoRequestHandlerBase::TransportAvailabilityInfo tai,
      PasswordCredentialFetcher::PasswordCredentials passwords);
  void FinishMaybeShowUI(
      PasswordCredentialFetcher::PasswordCredentials passwords,
      device::FidoRequestHandlerBase::TransportAvailabilityInfo tai);

  std::optional<device::FidoTransportProtocol> GetLastTransportUsed() const;

  void OnReadyForUI() override;

  // ShouldPermitCableExtension returns true if the given |origin| may set a
  // caBLE extension. This extension contains website-chosen BLE pairing
  // information that will be broadcast by the device.
  bool ShouldPermitCableExtension(const url::Origin& origin);

  void OnCableEvent(device::cablev2::Event event);

  // Adds GPM passkeys matching |rp_id| to |tai|.
  void GetPhoneContactableGpmPasskeysForRpId(
      device::FidoRequestHandlerBase::TransportAvailabilityInfo tai,
      base::OnceCallback<void(
          device::FidoRequestHandlerBase::TransportAvailabilityInfo)> callback);
  void DoGetPhoneContactableGpmPasskeysForRpId(
      device::FidoRequestHandlerBase::TransportAvailabilityInfo tai,
      base::OnceCallback<void(
          device::FidoRequestHandlerBase::TransportAvailabilityInfo)> callback);

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

  void OnPasswordSelected(password_manager::CredentialInfo info);

  void OnPasswordCredentialsReceived(
      PasswordCredentialFetcher::PasswordCredentials credentials);

  void UpdateModelForTransportAvailability(
      const device::FidoRequestHandlerBase::TransportAvailabilityInfo& tai);

  const content::GlobalRenderFrameHostId render_frame_host_id_;
  const scoped_refptr<AuthenticatorRequestDialogModel> dialog_model_;
  const std::unique_ptr<AuthenticatorRequestDialogController>
      dialog_controller_;
  base::OnceClosure cancel_callback_;
  base::OnceClosure immediate_not_found_callback_;
  base::RepeatingClosure start_over_callback_;
  AccountPreselectedCallback account_preselected_callback_;
  PasswordSelectedCallback password_selected_callback_;
  device::FidoRequestHandlerBase::RequestCallback request_callback_;
  base::OnceClosure cancel_ui_timeout_callback_;

  base::ScopedObservation<device::FidoRequestHandlerBase,
                          device::FidoRequestHandlerBase::Observer>
      request_handler_observation_{this};

  // The number of credential types that have been requested to be displayed.
  int credential_types_ =
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

  std::unique_ptr<GPMEnclaveController> enclave_controller_;

  std::unique_ptr<PasswordCredentialUIController> password_ui_controller_;
  std::unique_ptr<PasswordCredentialFetcher> password_fetcher_;

  // Stores the TransportAvailabilityInfo while we're waiting for the enclave
  // state to load from the disk.
  std::unique_ptr<device::FidoRequestHandlerBase::TransportAvailabilityInfo>
      pending_transport_availability_info_;

  // Stores the password credentials while waiting for enclave state, transport
  // availability info to be ready.
  std::unique_ptr<PasswordCredentialFetcher::PasswordCredentials>
      pending_password_credentials_;

  base::WeakPtrFactory<ChromeAuthenticatorRequestDelegate> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_WEBAUTHN_CHROME_AUTHENTICATOR_REQUEST_DELEGATE_H_
