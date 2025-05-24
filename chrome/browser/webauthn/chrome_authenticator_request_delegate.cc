// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/time/default_tick_clock.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/web_authentication_proxy/web_authentication_proxy_service.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/passwords/passwords_client_ui_delegate.h"
#include "chrome/browser/ui/webauthn/user_actions.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_controller.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/cablev2_devices.h"
#include "chrome/browser/webauthn/enclave_manager.h"
#include "chrome/browser/webauthn/gpm_enclave_controller.h"
#include "chrome/browser/webauthn/immediate_request_rate_limiter_factory.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "chrome/browser/webauthn/webauthn_metrics_util.h"
#include "chrome/browser/webauthn/webauthn_pref_names.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/pref_names.h"
#include "components/device_event_log/device_event_log.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/sync/service/sync_service.h"
#include "components/trusted_vault/frontend_trusted_vault_connection.h"
#include "components/user_prefs/user_prefs.h"
#include "components/webauthn/core/browser/immediate_request_rate_limiter.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_authentication_request_proxy.h"
#include "content/public/browser/web_contents.h"
#include "crypto/random.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/features.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "third_party/blink/public/mojom/credentialmanagement/credential_type_flags.mojom.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/native_widget_types.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate_mac.h"
#include "device/fido/mac/credential_metadata.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "ui/views/widget/widget.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/webauthn/local_credential_management_win.h"
#include "device/fido/win/authenticator.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/components/webauthn/webauthn_request_registrar.h"
#include "ui/aura/window.h"
#endif

using PasswordCredentials = PasswordCredentialController::PasswordCredentials;
using UIPresentation = ChromeAuthenticatorRequestDelegate::UIPresentation;
using TransportAvailabilityInfo =
    device::FidoRequestHandlerBase::TransportAvailabilityInfo;

namespace {

ChromeAuthenticatorRequestDelegate::TestObserver* g_observer = nullptr;

static constexpr char kGoogleRpId[] = "google.com";

// Returns true iff the credential is reported as being present on the platform
// authenticator (i.e. it is not a phone or icloud credential).
bool IsCredentialFromPlatformAuthenticator(
    device::DiscoverableCredentialMetadata cred) {
  return cred.source != device::AuthenticatorType::kICloudKeychain &&
         cred.source != device::AuthenticatorType::kPhone;
}

// Returns true iff |user_id| starts with the prefix reserved for passkeys used
// to authenticate to Google services.
bool UserIdHasGooglePasskeyAuthPrefix(const std::vector<uint8_t>& user_id) {
  constexpr std::string_view kPrefix = "GOOGLE_ACCOUNT:";
  if (user_id.size() < kPrefix.size()) {
    return false;
  }
  return UNSAFE_TODO(memcmp(user_id.data(), kPrefix.data(), kPrefix.size())) ==
         0;
}

// Filters |passkeys| to only contain credentials that are used to authenticate
// to Google services.
void FilterGoogleAuthPasskeys(
    std::vector<device::DiscoverableCredentialMetadata>* passkeys) {
  std::erase_if(*passkeys, [](const auto& passkey) {
    return IsCredentialFromPlatformAuthenticator(passkey) &&
           !UserIdHasGooglePasskeyAuthPrefix(passkey.user.id);
  });
}

#if BUILDFLAG(IS_MAC)
const char kWebAuthnTouchIdLastUsed[] = "webauthn.touchid.last_used";

// kMacOsRecentlyUsedMaxDays specifies how recently the macOS profile
// authenticator must have been used (for the current profile) to be considered
// "actively" used. Chrome may default to the profile authenticator in more
// cases if it is being actively used.
const int kMacOsRecentlyUsedMaxDays = 31;
#endif

// CableLinkingEventHandler handles linking information sent by caBLEv2
// authenticators. This linking information can come after the WebAuthn
// operation has resolved and thus after the
// `ChromeAuthenticatorRequestDelegate` has been destroyed. Thus this object is
// owned by the callback itself, and can save linking information until the
// point where the `Profile` itself is destroyed.
class CableLinkingEventHandler : public ProfileObserver {
 public:
  explicit CableLinkingEventHandler(Profile* profile) : profile_(profile) {
    profile_->AddObserver(this);
  }

  ~CableLinkingEventHandler() override {
    if (profile_) {
      profile_->RemoveObserver(this);
      profile_ = nullptr;
    }
  }

  void OnNewCablePairing(std::unique_ptr<device::cablev2::Pairing> pairing) {
    if (!profile_) {
      FIDO_LOG(DEBUG) << "Linking event was discarded because it was received "
                         "after the profile was destroyed.";
      return;
    }

    // Drop linking in Incognito sessions. While an argument could be made that
    // it's OK to persist them, this seems like the safe option.
    if (profile_->IsOffTheRecord()) {
      FIDO_LOG(DEBUG) << "Linking event was discarded because the profile is "
                         "Off The Record.";
      return;
    }

    cablev2::AddPairing(profile_, std::move(pairing));
  }

  // ProfileObserver:

  void OnProfileWillBeDestroyed(Profile* profile) override {
    DCHECK_EQ(profile, profile_);
    profile_->RemoveObserver(this);
    profile_ = nullptr;
  }

 private:
  raw_ptr<Profile> profile_;
};

bool SkipGpmPasskeyCreationForOwnAccount(
    device::FidoRequestType request_type,
    const std::string& rp_id,
    std::string_view user_name,
    const CoreAccountInfo& primary_account_info) {
  // Don't let GPM create a passkey for its own account within itself.
  //
  // The request username is either the full email address (GAIA users) or just
  // the local part (google.com users).
  //
  // Note that if the string does not contain an '@', `substr(0, npos)` will
  // return the whole string.
  const std::string account_email_local_part =
      primary_account_info.email.substr(0,
                                        primary_account_info.email.find('@'));
  return request_type == device::FidoRequestType::kMakeCredential &&
         rp_id == kGoogleRpId &&
         (user_name == primary_account_info.email ||
          user_name == account_email_local_part);
}

bool PasswordsUsable(int credential_types, UIPresentation ui_presentation) {
  if (!(credential_types &
        static_cast<int>(blink::mojom::CredentialTypeFlags::kPassword))) {
    return false;
  }

  if (base::FeatureList::IsEnabled(device::kWebAuthnAmbientSignin) &&
      ui_presentation == UIPresentation::kAutofill) {
    // TODO(https://crbug.com/358119268): This will probably get its own
    // mediation type, but for prototyping we assume any conditional request
    // with passwords uses ambient.
    return true;
  }

  return ui_presentation == UIPresentation::kModalImmediate;
}

}  // namespace

std::vector<std::unique_ptr<device::cablev2::Pairing>>
ChromeAuthenticatorRequestDelegate::TestObserver::
    GetCablePairingsFromSyncedDevices() {
  return {};
}

// static
void ChromeAuthenticatorRequestDelegate::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kSecurityKeyPermitAttestation);
  registry->RegisterIntegerPref(
      webauthn::pref_names::kEnclaveDeclinedGPMCredentialCreationCount, 0);
  registry->RegisterIntegerPref(
      webauthn::pref_names::kEnclaveDeclinedGPMBootstrappingCount, 0);
#if BUILDFLAG(IS_WIN)
  LocalCredentialManagementWin::RegisterProfilePrefs(registry);
#endif
#if BUILDFLAG(IS_MAC)
  registry->RegisterStringPref(
      webauthn::pref_names::kWebAuthnTouchIdMetadataSecretPrefName,
      std::string());
  registry->RegisterStringPref(kWebAuthnTouchIdLastUsed, std::string());
  // This boolean preference is used as a tristate. If unset, whether or not to
  // default to iCloud is determined based on several factors.
  // (See `ShouldCreateInICloudKeychain`.) If set, then this preference is
  // controlling.
  //
  // The default value of this preference only determines whether the toggle
  // in settings will show as set or not when the preference hasn't been
  // explicitly set. Since the behaviour is actually more complex than can be
  // expressed in a boolean, this is always an approximation.
  registry->RegisterBooleanPref(
      prefs::kCreatePasskeysInICloudKeychain,
      ShouldCreateInICloudKeychain(
          RequestSource::kWebAuthentication,
          // Whether or not the user is actively using the profile authenticator
          // is stored in preferences, which aren't available at this time while
          // we're still registering them. Thus we assume that they are not.
          /*is_active_profile_authenticator_user=*/false,
          IsICloudDriveEnabled(),
          /*request_is_for_google_com=*/false, /*preference=*/std::nullopt));
#endif
  cablev2::RegisterProfilePrefs(registry);
}

ChromeAuthenticatorRequestDelegate::ChromeAuthenticatorRequestDelegate(
    content::RenderFrameHost* render_frame_host)
    : render_frame_host_id_(render_frame_host->GetGlobalId()),
      dialog_model_(base::MakeRefCounted<AuthenticatorRequestDialogModel>(
          GetRenderFrameHost())),
      dialog_controller_(std::make_unique<AuthenticatorRequestDialogController>(
          dialog_model_.get(),
          GetRenderFrameHost())) {
  dialog_model_->observers.AddObserver(this);
  if (g_observer) {
    g_observer->Created(this);
  }
}

ChromeAuthenticatorRequestDelegate::~ChromeAuthenticatorRequestDelegate() {
  // Currently, completion of the request is indicated by //content destroying
  // this delegate.
  dialog_model_->OnRequestComplete();
  dialog_model_->observers.RemoveObserver(this);

  if (g_observer) {
    g_observer->OnDestroy(this);
  }
}

// static
void ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(
    TestObserver* observer) {
  CHECK(!observer || !g_observer);
  g_observer = observer;
}

base::WeakPtr<ChromeAuthenticatorRequestDelegate>
ChromeAuthenticatorRequestDelegate::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

GPMEnclaveController*
ChromeAuthenticatorRequestDelegate::enclave_controller_for_testing() const {
  return enclave_controller_.get();
}

void ChromeAuthenticatorRequestDelegate::SetRelyingPartyId(
    const std::string& rp_id) {
  dialog_model_->relying_party_id = rp_id;
}

void ChromeAuthenticatorRequestDelegate::SetUIPresentation(
    UIPresentation ui_presentation) {
  dialog_controller_->SetUIPresentation(ui_presentation);
}

bool ChromeAuthenticatorRequestDelegate::DoesBlockRequestOnFailure(
    InterestingFailureReason reason) {
  if (!webauthn_ui_enabled()) {
    return false;
  }

  // If the UI was already in the state where we asked the user to complete the
  // transaction on the other device then any errors are immediately resolved.
  // Very likely the user canceled on the phone and doesn't want to see another
  // error UI on the desktop.
  if (cable_device_ready_) {
    return false;
  }

  switch (reason) {
    case InterestingFailureReason::kTimeout:
      dialog_controller_->OnRequestTimeout();
      break;
    case InterestingFailureReason::kKeyNotRegistered:
      dialog_controller_->OnActivatedKeyNotRegistered();
      break;
    case InterestingFailureReason::kKeyAlreadyRegistered:
      dialog_controller_->OnActivatedKeyAlreadyRegistered();
      break;
    case InterestingFailureReason::kSoftPINBlock:
      dialog_controller_->OnSoftPINBlock();
      break;
    case InterestingFailureReason::kHardPINBlock:
      dialog_controller_->OnHardPINBlock();
      break;
    case InterestingFailureReason::kAuthenticatorRemovedDuringPINEntry:
      dialog_controller_->OnAuthenticatorRemovedDuringPINEntry();
      break;
    case InterestingFailureReason::kAuthenticatorMissingResidentKeys:
      dialog_controller_->OnAuthenticatorMissingResidentKeys();
      break;
    case InterestingFailureReason::kAuthenticatorMissingUserVerification:
      dialog_controller_->OnAuthenticatorMissingUserVerification();
      break;
    case InterestingFailureReason::kAuthenticatorMissingLargeBlob:
      dialog_controller_->OnAuthenticatorMissingLargeBlob();
      break;
    case InterestingFailureReason::kNoCommonAlgorithms:
      dialog_controller_->OnNoCommonAlgorithms();
      break;
    case InterestingFailureReason::kStorageFull:
      dialog_controller_->OnAuthenticatorStorageFull();
      break;
    case InterestingFailureReason::kUserConsentDenied:
      dialog_controller_->OnUserConsentDenied();
      break;
    case InterestingFailureReason::kWinUserCancelled:
      return dialog_controller_->OnWinUserCancelled();
    case InterestingFailureReason::kHybridTransportError:
      return dialog_controller_->OnHybridTransportError();
    case InterestingFailureReason::kNoPasskeys:
      return dialog_controller_->OnNoPasskeys();
    case InterestingFailureReason::kEnclaveError:
      return dialog_controller_->OnEnclaveError();
    case InterestingFailureReason::kEnclaveCancel:
      dialog_model_->CancelAuthenticatorRequest();
      break;
    case InterestingFailureReason::kChallengeUrlFailure:
      dialog_controller_->OnChallengeUrlFailure();
  }
  return true;
}

void ChromeAuthenticatorRequestDelegate::OnTransactionSuccessful(
    RequestSource request_source,
    device::FidoRequestType request_type,
    device::AuthenticatorType authenticator_type) {
  if (request_source != RequestSource::kWebAuthentication) {
    return;
  }
#if BUILDFLAG(IS_MAC)
  if (authenticator_type == device::AuthenticatorType::kTouchID) {
    profile()->GetPrefs()->SetString(
        kWebAuthnTouchIdLastUsed,
        base::UnlocalizedTimeFormatWithPattern(base::Time::Now(), "yyyy-MM-dd",
                                               icu::TimeZone::getGMT()));
    webauthn::user_actions::RecordChromeProfileSuccess();
  }
  if (authenticator_type == device::AuthenticatorType::kICloudKeychain) {
    webauthn::user_actions::RecordICloudSuccess();
  }

  dialog_controller_->RecordMacOsSuccessHistogram(request_type,
                                                  authenticator_type);
#elif BUILDFLAG(IS_WIN)
  if (authenticator_type == device::AuthenticatorType::kWinNative) {
    webauthn::user_actions::RecordWindowsHelloSuccess();
  }
#endif  // BUILDFLAG(IS_MAC)
  if (authenticator_type == device::AuthenticatorType::kEnclave) {
    if (dialog_model_->in_onboarding_flow) {
      RecordOnboardingEvent(webauthn::metrics::OnboardingEvents::kSucceeded);
    }
    switch (request_type) {
      case device::FidoRequestType::kGetAssertion:
        RecordGPMGetAssertionEvent(
            webauthn::metrics::GPMGetAssertionEvents::kSuccess);
        break;
      case device::FidoRequestType::kMakeCredential:
        RecordGPMMakeCredentialEvent(
            webauthn::metrics::GPMMakeCredentialEvents::kSuccess);
        break;
    }
    webauthn::user_actions::RecordGpmSuccess();
  }
}

void ChromeAuthenticatorRequestDelegate::RegisterActionCallbacks(
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
        request_ble_permission_callback) {
  cancel_callback_ = std::move(cancel_callback);
  immediate_not_found_callback_ = std::move(immediate_not_found_callback);
  start_over_callback_ = std::move(start_over_callback);
  account_preselected_callback_ = std::move(account_preselected_callback);
  password_selected_callback_ = std::move(password_selected_callback);
  request_callback_ = request_callback;
  cancel_ui_timeout_callback_ = std::move(cancel_ui_timeout_callback);

  dialog_controller_->SetRequestCallback(request_callback);
  dialog_controller_->SetAccountPreselectedCallback(
      account_preselected_callback_);
  dialog_controller_->SetBluetoothAdapterPowerOnCallback(
      bluetooth_adapter_power_on_callback);
  dialog_controller_->SetRequestBlePermissionCallback(
      request_ble_permission_callback);
  if (password_controller_) {
    password_controller_->SetPasswordSelectedCallback(
        password_selected_callback_);
  }
}

void ChromeAuthenticatorRequestDelegate::ConfigureDiscoveries(
    const url::Origin& origin,
    const std::string& rp_id,
    RequestSource request_source,
    device::FidoRequestType request_type,
    std::optional<device::ResidentKeyRequirement> resident_key_requirement,
    device::UserVerificationRequirement user_verification_requirement,
    std::optional<std::string_view> user_name,
    base::span<const device::CableDiscoveryData> pairings_from_extension,
    bool browser_provided_passkeys_available,
    device::FidoDiscoveryFactory* discovery_factory) {
  DCHECK(request_type == device::FidoRequestType::kGetAssertion ||
         resident_key_requirement.has_value());

  // Without the UI enabled, discoveries like caBLE, Android AOA, iCloud
  // keychain, and the enclave, don't make sense.
  if (!webauthn_ui_enabled()) {
    return;
  }

  // Configure the enclave authenticator.
  if (browser_provided_passkeys_available && !IsVirtualEnvironmentEnabled() &&
      request_source == RequestSource::kWebAuthentication) {
    // Creating credentials in GPM can be disabled by policy, but get() is
    // always allowed.
    const bool enclave_create_enabled =
        profile()->GetPrefs()->GetBoolean(
            password_manager::prefs::kCredentialsEnableService) &&
        profile()->GetPrefs()->GetBoolean(
            password_manager::prefs::kCredentialsEnablePasskeys);
    if (dialog_controller_->ui_presentation() ==
            UIPresentation::kPasskeyUpgrade &&
        enclave_create_enabled) {
      // PasskeyUpgradeRequestController will handle enclave transactions in
      // place of the "regular" GPMEnclaveController.
      CHECK(!enclave_controller_);
      dialog_controller_->InitializeEnclaveRequestCallback(discovery_factory);
      discovery_factory->set_network_context_factory(base::BindRepeating([]() {
        return SystemNetworkContextManager::GetInstance()->GetContext();
      }));
    } else if (request_type == device::FidoRequestType::kGetAssertion ||
               enclave_create_enabled) {
      // Set up the "regular" enclave controller.
      auto* const identity_manager = IdentityManagerFactory::GetForProfile(
          profile()->GetOriginalProfile());
      const auto consent = signin::ConsentLevel::kSignin;
      if (identity_manager->HasPrimaryAccount(consent)) {
        CoreAccountInfo account_info =
            identity_manager->GetPrimaryAccountInfo(consent);
        if (SkipGpmPasskeyCreationForOwnAccount(
                request_type, rp_id, user_name.value_or(""), account_info)) {
          FIDO_LOG(EVENT)
              << "Creation in GPM not offered (same primary account)";
        } else {
          enclave_controller_ = std::make_unique<GPMEnclaveController>(
              GetRenderFrameHost(), dialog_model_.get(), rp_id, request_type,
              user_verification_requirement,
              tick_clock_ ? tick_clock_ : base::DefaultTickClock::GetInstance(),
              timer_task_runner_, std::move(pending_trusted_vault_connection_));
        }
      }
    } else {
      FIDO_LOG(EVENT)
          << "Enclave unavailable for creating passkeys due to policy.";
    }
  }

  const bool cable_extension_permitted = ShouldPermitCableExtension(origin);
  const bool cable_extension_provided =
      cable_extension_permitted && !pairings_from_extension.empty();

  if (g_observer) {
    for (const auto& pairing : pairings_from_extension) {
      if (pairing.version == device::CableDiscoveryData::Version::V2) {
        g_observer->CableV2ExtensionSeen(pairing.v2->server_link_data);
      }
    }

    g_observer->ConfiguringCable(request_type);
  }

#if BUILDFLAG(IS_LINUX)
  // No caBLEv1 on Linux. It tends to crash bluez.
  if (base::Contains(pairings_from_extension,
                     device::CableDiscoveryData::Version::V1,
                     &device::CableDiscoveryData::version)) {
    pairings_from_extension = base::span<const device::CableDiscoveryData>();
  }
#endif

  std::vector<device::CableDiscoveryData> pairings;
  if (cable_extension_permitted) {
    pairings.insert(pairings.end(), pairings_from_extension.begin(),
                    pairings_from_extension.end());
  }
  const bool cable_extension_accepted = !pairings.empty();
  const bool cablev2_extension_provided =
      base::Contains(pairings, device::CableDiscoveryData::Version::V2,
                     &device::CableDiscoveryData::version);

  std::vector<std::unique_ptr<device::cablev2::Pairing>> paired_phones;
  base::RepeatingCallback<void(std::unique_ptr<device::cablev2::Pairing>)>
      contact_phone_callback;
  if (base::FeatureList::IsEnabled(device::kWebAuthnHybridLinking) &&
      (!cable_extension_provided ||
       base::FeatureList::IsEnabled(device::kWebAuthCableExtensionAnywhere))) {
    std::unique_ptr<cablev2::KnownDevices> known_devices =
        cablev2::KnownDevices::FromProfile(profile());
    if (g_observer) {
      known_devices->synced_devices =
          g_observer->GetCablePairingsFromSyncedDevices();
    }
    can_use_synced_phone_passkeys_ = !known_devices->synced_devices.empty();
    paired_phones = cablev2::MergeDevices(std::move(known_devices),
                                          &icu::Locale::getDefault());

    // The debug log displays in reverse order, so the headline is emitted after
    // the names.
    for (const auto& pairing : paired_phones) {
      FIDO_LOG(DEBUG) << "• " << pairing->name << " " << pairing->last_updated
                      << " priority:" << pairing->channel_priority;
    }
    FIDO_LOG(DEBUG) << "Found " << paired_phones.size() << " caBLEv2 devices";

    if (!paired_phones.empty()) {
      contact_phone_callback = discovery_factory->get_cable_contact_callback();
    }
  }

  const bool non_extension_cablev2_enabled =
      (!cable_extension_permitted ||
       (!cable_extension_provided &&
        request_type == device::FidoRequestType::kGetAssertion) ||
       (request_type == device::FidoRequestType::kMakeCredential &&
        resident_key_requirement.has_value() &&
        resident_key_requirement.value() !=
            device::ResidentKeyRequirement::kDiscouraged) ||
       base::FeatureList::IsEnabled(device::kWebAuthCableExtensionAnywhere));

  std::optional<std::array<uint8_t, device::cablev2::kQRKeySize>>
      qr_generator_key;
  std::optional<std::string> qr_string;
  if (non_extension_cablev2_enabled || cablev2_extension_provided) {
    // A QR key is generated for all caBLEv2 cases but whether the QR code is
    // displayed is up to the UI.
    qr_generator_key.emplace();
    crypto::RandBytes(*qr_generator_key);
    qr_string = device::cablev2::qr::Encode(*qr_generator_key, request_type);

    auto linking_handler =
        std::make_unique<CableLinkingEventHandler>(profile());
    discovery_factory->set_cable_pairing_callback(
        base::BindRepeating(&CableLinkingEventHandler::OnNewCablePairing,
                            std::move(linking_handler)));
    discovery_factory->set_cable_invalidated_pairing_callback(
        base::BindRepeating(
            &ChromeAuthenticatorRequestDelegate::OnInvalidatedCablePairing,
            weak_ptr_factory_.GetWeakPtr()));
    discovery_factory->set_cable_event_callback(
        base::BindRepeating(&ChromeAuthenticatorRequestDelegate::OnCableEvent,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  if (SystemNetworkContextManager::GetInstance()) {
    // caBLE and the enclave depend on the network context factory.
    // TODO(nsatragno): this should probably use a storage partition network
    // context instead. See the SystemNetworkContextManager class comments.
    discovery_factory->set_network_context_factory(base::BindRepeating([]() {
      return SystemNetworkContextManager::GetInstance()->GetContext();
    }));
  }

  if (cable_extension_accepted || non_extension_cablev2_enabled) {
    std::optional<bool> extension_is_v2;
    if (cable_extension_provided) {
      extension_is_v2 = cablev2_extension_provided;
    }
    dialog_controller_->set_cable_transport_info(
        extension_is_v2, std::move(paired_phones),
        std::move(contact_phone_callback), qr_string);
    discovery_factory->set_cable_data(request_type, std::move(pairings),
                                      qr_generator_key);
  }

#if BUILDFLAG(IS_MAC)
  ConfigureNSWindow(discovery_factory);
#endif

  if (enclave_controller_) {
    enclave_controller_->ConfigureDiscoveries(discovery_factory);
  }

  dialog_controller_->set_is_non_webauthn_request(
      request_source != RequestSource::kWebAuthentication);

#if BUILDFLAG(IS_MAC)
  ConfigureICloudKeychain(request_source, rp_id);
#endif

  if (PasswordsUsable(credential_types_,
                      dialog_controller_->ui_presentation())) {
    // Only valid for the main frame.
    if (!password_controller_ && GetRenderFrameHost()->IsInPrimaryMainFrame()) {
      password_controller_ = std::make_unique<PasswordCredentialController>(
          render_frame_host_id_, dialog_model_.get());
    }
    if (!password_controller_) {
      return;
    }
    password_controller_->FetchPasswords(
        origin.GetURL(),
        base::BindOnce(
            &ChromeAuthenticatorRequestDelegate::OnPasswordCredentialsReceived,
            AsWeakPtr()));
  }
}

void ChromeAuthenticatorRequestDelegate::SetHints(
    const AuthenticatorRequestClientDelegate::Hints& hints) {
  if (g_observer) {
    g_observer->HintsSet(hints);
  }
  dialog_controller_->SetHints(hints);
}

void ChromeAuthenticatorRequestDelegate::SelectAccount(
    std::vector<device::AuthenticatorGetAssertionResponse> responses,
    base::OnceCallback<void(device::AuthenticatorGetAssertionResponse)>
        callback) {
  if (!webauthn_ui_enabled()) {
    // Requests with UI disabled should never reach account selection.
    DCHECK(IsVirtualEnvironmentEnabled());

    // The browser is being automated. Select the first credential to support
    // automation of discoverable credentials.
    // TODO(crbug.com/40639383): Provide a way to determine which account gets
    // picked.
    std::move(callback).Run(std::move(responses.at(0)));
    return;
  }

  if (g_observer) {
    g_observer->AccountSelectorShown(responses);
    std::move(callback).Run(std::move(responses.at(0)));
    return;
  }

  dialog_controller_->SelectAccount(std::move(responses), std::move(callback));
}

bool ChromeAuthenticatorRequestDelegate::webauthn_ui_enabled() const {
  return dialog_controller_->ui_presentation() != UIPresentation::kDisabled;
}

void ChromeAuthenticatorRequestDelegate::SetCredentialTypes(
    int credential_type_flags) {
  credential_types_ = credential_type_flags;
}

void ChromeAuthenticatorRequestDelegate::SetCredentialIdFilter(
    std::vector<device::PublicKeyCredentialDescriptor> credential_list) {
  credential_filter_ = std::move(credential_list);
}

void ChromeAuthenticatorRequestDelegate::SetUserEntityForMakeCredentialRequest(
    const device::PublicKeyCredentialUserEntity& user_entity) {
  dialog_model_->user_entity = user_entity;
}

void ChromeAuthenticatorRequestDelegate::ProvideChallengeUrl(
    const GURL& url,
    base::OnceCallback<void(std::optional<base::span<const uint8_t>>)>
        callback) {
  dialog_controller_->ProvideChallengeUrl(url, std::move(callback));
}

void ChromeAuthenticatorRequestDelegate::OnTransportAvailabilityEnumerated(
    TransportAvailabilityInfo data) {
  if (g_observer) {
    g_observer->OnPreTransportAvailabilityEnumerated(this);
  }

  if (!webauthn_ui_enabled()) {
    return;
  }

  pending_transport_availability_info_ = std::make_unique<
      device::FidoRequestHandlerBase::TransportAvailabilityInfo>(
      std::move(data));
  TryToShowUI();
}

bool ChromeAuthenticatorRequestDelegate::EmbedderControlsAuthenticatorDispatch(
    const device::FidoAuthenticator& authenticator) {
  // Decide whether the //device/fido code should dispatch the current
  // request to an authenticator immediately after it has been
  // discovered, or whether the embedder/UI takes charge of that by
  // invoking its RequestCallback.
  if (!webauthn_ui_enabled()) {
    // There is no UI to handle request dispatch.
    return false;
  }
  if (authenticator.GetType() == device::AuthenticatorType::kEnclave) {
    return false;
  }

  if (dialog_controller_->ui_presentation() == UIPresentation::kAutofill &&
      (dialog_model_->step() ==
           AuthenticatorRequestDialogModel::Step::kPasskeyAutofill ||
       dialog_model_->step() ==
           AuthenticatorRequestDialogModel::Step::kNotStarted)) {
    // There is an active conditional request that is not showing any UI. The UI
    // will dispatch to any plugged in authenticators after the user selects an
    // option.
    return true;
  }
  auto transport = authenticator.AuthenticatorTransport();
  return !transport ||  // Windows
         *transport == device::FidoTransportProtocol::kInternal;
}

void ChromeAuthenticatorRequestDelegate::FidoAuthenticatorAdded(
    const device::FidoAuthenticator& authenticator) {
  if (!webauthn_ui_enabled()) {
    return;
  }

  dialog_controller_->AddAuthenticator(authenticator);
}

void ChromeAuthenticatorRequestDelegate::FidoAuthenticatorRemoved(
    std::string_view authenticator_id) {
  if (!webauthn_ui_enabled()) {
    return;
  }

  dialog_controller_->RemoveAuthenticator(authenticator_id);
}

void ChromeAuthenticatorRequestDelegate::BluetoothAdapterStatusChanged(
    device::FidoRequestHandlerBase::BleStatus ble_status) {
  dialog_controller_->BluetoothAdapterStatusChanged(ble_status);
}

bool ChromeAuthenticatorRequestDelegate::SupportsPIN() const {
  return true;
}

void ChromeAuthenticatorRequestDelegate::CollectPIN(
    CollectPINOptions options,
    base::OnceCallback<void(std::u16string)> provide_pin_cb) {
  dialog_controller_->CollectPIN(options.reason, options.error,
                                 options.min_pin_length, options.attempts,
                                 std::move(provide_pin_cb));
}

void ChromeAuthenticatorRequestDelegate::StartBioEnrollment(
    base::OnceClosure next_callback) {
  dialog_controller_->StartInlineBioEnrollment(std::move(next_callback));
}

void ChromeAuthenticatorRequestDelegate::OnSampleCollected(
    int bio_samples_remaining) {
  dialog_controller_->OnSampleCollected(bio_samples_remaining);
}

void ChromeAuthenticatorRequestDelegate::FinishCollectToken() {
  dialog_controller_->FinishCollectToken();
}

void ChromeAuthenticatorRequestDelegate::OnRetryUserVerification(int attempts) {
  dialog_controller_->OnRetryUserVerification(attempts);
}

void ChromeAuthenticatorRequestDelegate::OnStartOver() {
  DCHECK(start_over_callback_);
  dialog_model_->generation++;
  if (g_observer) {
    g_observer->PreStartOver();
  }
  start_over_callback_.Run();
}

void ChromeAuthenticatorRequestDelegate::OnModelDestroyed(
    AuthenticatorRequestDialogModel* model) {
  DCHECK_EQ(model, dialog_model_.get());
}

void ChromeAuthenticatorRequestDelegate::OnCancelRequest() {
  // |cancel_callback_| must be invoked at most once as invocation of
  // |cancel_callback_| will destroy |this|.
  DCHECK(cancel_callback_);
  std::move(cancel_callback_).Run();
}

void ChromeAuthenticatorRequestDelegate::OnManageDevicesClicked() {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(GetRenderFrameHost());
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (browser) {
    NavigateParams params(browser,
                          GURL("chrome://settings/securityKeys/phones"),
                          ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL);
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    Navigate(&params);
  }
}

void ChromeAuthenticatorRequestDelegate::SetTrustedVaultConnectionForTesting(
    std::unique_ptr<trusted_vault::TrustedVaultConnection> connection) {
  pending_trusted_vault_connection_ = std::move(connection);
}

void ChromeAuthenticatorRequestDelegate::SetMockTimeForTesting(
    base::TickClock const* tick_clock,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  CHECK(!enclave_controller_);
  tick_clock_ = tick_clock;
  timer_task_runner_ = std::move(task_runner);
}

void ChromeAuthenticatorRequestDelegate::SetPasswordControllerForTesting(
    std::unique_ptr<PasswordCredentialController> controller) {
  password_controller_ = std::move(controller);
}

content::RenderFrameHost*
ChromeAuthenticatorRequestDelegate::GetRenderFrameHost() const {
  content::RenderFrameHost* ret =
      content::RenderFrameHost::FromID(render_frame_host_id_);
  DCHECK(ret);
  return ret;
}

content::BrowserContext* ChromeAuthenticatorRequestDelegate::GetBrowserContext()
    const {
  return GetRenderFrameHost()->GetBrowserContext();
}

Profile* ChromeAuthenticatorRequestDelegate::profile() const {
  return Profile::FromBrowserContext(GetRenderFrameHost()->GetBrowserContext());
}

bool ChromeAuthenticatorRequestDelegate::MaybeHandleImmediateMediation(
    const TransportAvailabilityInfo& data,
    const PasswordCredentials& passwords) {
  if (data.request_type != device::FidoRequestType::kGetAssertion ||
      dialog_controller_->ui_presentation() !=
          UIPresentation::kModalImmediate) {
    return false;
  }

  // Always return not allowed immediate in incognito.
  if (profile()->IsOffTheRecord()) {
    return true;
  }

  if (auto* rate_limiter =
          ImmediateRequestRateLimiterFactory::GetForProfile(profile())) {
    const url::Origin origin = GetRenderFrameHost()->GetLastCommittedOrigin();
    if (!rate_limiter->IsRequestAllowed(origin)) {
      FIDO_LOG(ERROR)
          << "Immediate request rate limit exceeded for the origin.";
      return true;
    }
  }

  if (data.recognized_credentials.size() + passwords.size() == 0) {
    return true;
  }

  return false;
}

void ChromeAuthenticatorRequestDelegate::TryToShowUI() {
  if (!pending_transport_availability_info_) {
    return;
  }
  if (enclave_controller_ && !enclave_controller_->ready_for_ui()) {
    // Delay showing UI until GPM state is loaded. It's only after this
    // point that we know whether GPM will be active for this request or not.
    return;
  }
  if (PasswordsUsable(credential_types_,
                      dialog_controller_->ui_presentation()) &&
      !pending_password_credentials_) {
    return;
  }
  auto tai = std::move(pending_transport_availability_info_);
  auto passwords = pending_password_credentials_
                       ? std::move(pending_password_credentials_)
                       : std::make_unique<PasswordCredentials>();
  MaybeShowUI(std::move(*tai), std::move(*passwords));
}

void ChromeAuthenticatorRequestDelegate::MaybeShowUI(
    TransportAvailabilityInfo tai,
    PasswordCredentials passwords) {
  if (can_use_synced_phone_passkeys_ ||
      (enclave_controller_ && enclave_controller_->is_active())) {
    GetPhoneContactableGpmPasskeysForRpId(&tai.recognized_credentials);
  }
  FilterRecognizedCredentials(&tai);

  if (MaybeHandleImmediateMediation(tai, passwords)) {
    std::move(immediate_not_found_callback_).Run();
    return;
  }

  if (!cancel_ui_timeout_callback_.is_null()) {
    std::move(cancel_ui_timeout_callback_).Run();
  }

  if (g_observer) {
    g_observer->OnTransportAvailabilityEnumerated(this, &tai);
  }

  if (dialog_model_->step() !=
      AuthenticatorRequestDialogModel::Step::kNotStarted) {
    return;
  }

  dialog_controller_->SetCredentialTypes(credential_types_);
  UpdateModelForTransportAvailability(tai);

  // Precalculate the UV method for immediate mode requests.
  dialog_model_->gpm_uv_method.reset();
  if (enclave_controller_) {
    dialog_model_->gpm_uv_method =
        enclave_controller_->GetEnclaveUserVerificationMethod();
  }

  dialog_controller_->StartFlow(std::move(tai), std::move(passwords));

  if (g_observer) {
    g_observer->UIShown(this);
  }
}

void ChromeAuthenticatorRequestDelegate::OnReadyForUI() {
  TryToShowUI();
}

bool ChromeAuthenticatorRequestDelegate::ShouldPermitCableExtension(
    const url::Origin& origin) {
  if (base::FeatureList::IsEnabled(device::kWebAuthCableExtensionAnywhere)) {
    return true;
  }

  // Because the future of the caBLE extension might be that we transition
  // everything to QR-code or sync-based pairing, we don't want use of the
  // extension to spread without consideration. Therefore it's limited to
  // origins that are already depending on it and test sites.
  if (origin.DomainIs("google.com")) {
    return true;
  }

  const GURL test_site("https://webauthndemo.appspot.com");
  DCHECK(test_site.is_valid());
  return origin.IsSameOriginWith(test_site);
}

void ChromeAuthenticatorRequestDelegate::OnInvalidatedCablePairing(
    std::unique_ptr<device::cablev2::Pairing> failed_pairing) {
  // A pairing was reported to be invalid. Delete it unless it came from Sync,
  // in which case there's nothing to be done.
  cablev2::DeletePairingByPublicKey(profile()->GetPrefs(),
                                    failed_pairing->peer_public_key_x962);

  // Contact the next phone with the same name, if any, given that no
  // notification has been sent.
  dialog_controller_->OnPhoneContactFailed(failed_pairing->name);
}

void ChromeAuthenticatorRequestDelegate::OnCableEvent(
    device::cablev2::Event event) {
  if (event == device::cablev2::Event::kReady) {
    cable_device_ready_ = true;
  }

  dialog_controller_->OnCableEvent(event);
}

void ChromeAuthenticatorRequestDelegate::GetPhoneContactableGpmPasskeysForRpId(
    std::vector<device::DiscoverableCredentialMetadata>* passkeys) {
  device::AuthenticatorType type;
  std::vector<sync_pb::WebauthnCredentialSpecifics> credentials;

  if (enclave_controller_ && enclave_controller_->is_active()) {
    credentials = enclave_controller_->creds();
    type = device::AuthenticatorType::kEnclave;
  } else {
    webauthn::PasskeyModel* passkey_model =
        PasskeyModelFactory::GetInstance()->GetForProfile(profile());
    CHECK(passkey_model);
    credentials = passkey_model->GetPasskeysForRelyingPartyId(
        dialog_model_->relying_party_id);
    type = device::AuthenticatorType::kPhone;
  }

  if (dialog_controller_->ui_presentation() ==
          UIPresentation::kModalImmediate &&
      !credentials.empty()) {
    if (enclave_controller_ && !enclave_controller_->is_account_ready()) {
      base::UmaHistogramBoolean(
          "WebAuthentication.GetAssertion.Immediate.EnclaveReady", false);
      return;
    }
    base::UmaHistogramBoolean(
        "WebAuthentication.GetAssertion.Immediate.EnclaveReady", true);
  }

  for (const sync_pb::WebauthnCredentialSpecifics& passkey : credentials) {
    const base::Time last_used_time = base::Time::FromDeltaSinceWindowsEpoch(
        base::Microseconds(passkey.last_used_time_windows_epoch_micros()));
    const base::Time creation_time =
        base::Time::FromMillisecondsSinceUnixEpoch(passkey.creation_time());
    passkeys->emplace_back(
        type, passkey.rp_id(),
        std::vector<uint8_t>(passkey.credential_id().begin(),
                             passkey.credential_id().end()),
        device::PublicKeyCredentialUserEntity(
            std::vector<uint8_t>(passkey.user_id().begin(),
                                 passkey.user_id().end()),
            passkey.user_name(), passkey.user_display_name()),
        /*provider_name=*/std::nullopt,
        last_used_time > creation_time ? last_used_time : creation_time);
  }
}

void ChromeAuthenticatorRequestDelegate::FilterRecognizedCredentials(
    TransportAvailabilityInfo* tai) {
  if (dialog_model()->relying_party_id == kGoogleRpId &&
      tai->has_empty_allow_list &&
      std::ranges::any_of(tai->recognized_credentials,
                          IsCredentialFromPlatformAuthenticator)) {
    // Regrettably, Chrome will create webauthn credentials for things other
    // than authentication (e.g. credit card autofill auth) under the rp id
    // "google.com". To differentiate those credentials from actual passkeys you
    // can use to sign in, Google adds a prefix to the user id.
    // This code filter passkeys that do not match that prefix.
    FilterGoogleAuthPasskeys(&tai->recognized_credentials);
    if (tai->has_platform_authenticator_credential ==
            device::FidoRequestHandlerBase::RecognizedCredential::
                kHasRecognizedCredential &&
        std::ranges::none_of(tai->recognized_credentials,
                             IsCredentialFromPlatformAuthenticator)) {
      tai->has_platform_authenticator_credential = device::
          FidoRequestHandlerBase::RecognizedCredential::kNoRecognizedCredential;
    }
  }

  if (!credential_filter_.empty()) {
    std::vector<device::DiscoverableCredentialMetadata> filtered_list;
    for (auto& platform_credential : tai->recognized_credentials) {
      for (auto& filter_credential : credential_filter_) {
        if (platform_credential.cred_id == filter_credential.id) {
          filtered_list.push_back(platform_credential);
          break;
        }
      }
    }
    tai->recognized_credentials = std::move(filtered_list);
  }

  const auto kImmediateTypes =
      std::unordered_set{device::AuthenticatorType::kEnclave,
                         device::AuthenticatorType::kICloudKeychain,
                         device::AuthenticatorType::kWinNative,
                         device::AuthenticatorType::kChromeOS,
                         device::AuthenticatorType::kTouchID};
  if (dialog_controller_->ui_presentation() ==
      UIPresentation::kModalImmediate) {
    std::erase_if(tai->recognized_credentials,
                  [&kImmediateTypes](const auto& passkey) {
                    return !kImmediateTypes.contains(passkey.source);
                  });
  }
}

#if BUILDFLAG(IS_MAC)
// static
std::optional<int> ChromeAuthenticatorRequestDelegate::DaysSinceDate(
    const std::string& formatted_date,
    const base::Time now) {
  int year, month, day_of_month;
  // sscanf will ignore trailing garbage, but we don't need to be strict here.
  if (UNSAFE_TODO(sscanf(formatted_date.c_str(), "%u-%u-%u", &year, &month,
                         &day_of_month)) != 3) {
    return std::nullopt;
  }

  const base::Time::Exploded exploded = {
      .year = year, .month = month, .day_of_month = day_of_month};

  base::Time t;
  if (!base::Time::FromUTCExploded(exploded, &t) || now < t) {
    return std::nullopt;
  }

  const base::TimeDelta difference = now - t;
  return difference.InDays();
}

// static
std::optional<bool> ChromeAuthenticatorRequestDelegate::GetICloudKeychainPref(
    const PrefService* prefs) {
  const PrefService::Preference* pref =
      prefs->FindPreference(prefs::kCreatePasskeysInICloudKeychain);
  if (pref->IsDefaultValue()) {
    return std::nullopt;
  }
  return pref->GetValue()->GetBool();
}

// static
bool ChromeAuthenticatorRequestDelegate::IsActiveProfileAuthenticatorUser(
    const PrefService* prefs) {
  const std::string& last_used = prefs->GetString(kWebAuthnTouchIdLastUsed);
  if (last_used.empty()) {
    return false;
  }
  const std::optional<int> days = DaysSinceDate(last_used, base::Time::Now());
  return days.has_value() && days.value() <= kMacOsRecentlyUsedMaxDays;
}

// static
bool ChromeAuthenticatorRequestDelegate::ShouldCreateInICloudKeychain(
    RequestSource request_source,
    bool is_active_profile_authenticator_user,
    bool has_icloud_drive_enabled,
    bool request_is_for_google_com,
    std::optional<bool> preference) {
  // Secure Payment Confirmation and credit-card autofill continue to use
  // the profile authenticator.
  if (request_source != RequestSource::kWebAuthentication) {
    return false;
  }
  if (preference.has_value()) {
    return *preference;
  }
  const base::Feature* feature;
  if (request_is_for_google_com) {
    feature = &device::kWebAuthnICloudKeychainForGoogle;
  } else {
    if (is_active_profile_authenticator_user) {
      if (has_icloud_drive_enabled) {
        feature = &device::kWebAuthnICloudKeychainForActiveWithDrive;
      } else {
        feature = &device::kWebAuthnICloudKeychainForActiveWithoutDrive;
      }
    } else {
      if (has_icloud_drive_enabled) {
        feature = &device::kWebAuthnICloudKeychainForInactiveWithDrive;
      } else {
        feature = &device::kWebAuthnICloudKeychainForInactiveWithoutDrive;
      }
    }
  }

  return base::FeatureList::IsEnabled(*feature);
}

void ChromeAuthenticatorRequestDelegate::ConfigureNSWindow(
    device::FidoDiscoveryFactory* discovery_factory) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(GetRenderFrameHost());
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (browser && browser->is_type_app()) {
    // PWAs render the UI in an out-of-process window, thus there is no valid
    // NSWindow* available in the browser process.
    // TODO: crbug.com/364926914 - potentially do iCloud Keychain operations out
    // of process so that they can work in PWAs.
    return;
  }

  // Not all contexts in which this code runs have a BrowserWindow.
  // Notably the dialog containing a WebContents that is used for signing
  // into a new profile does not. Thus the NSWindow is fetched more directly.
  const views::Widget* widget = views::Widget::GetTopLevelWidgetForNativeView(
      web_contents->GetNativeView());
  if (widget) {
    const gfx::NativeWindow window = widget->GetNativeWindow();
    if (window) {
      discovery_factory->set_nswindow(window);
    }
  }
}
void ChromeAuthenticatorRequestDelegate::ConfigureICloudKeychain(
    RequestSource request_source,
    const std::string& rp_id) {
  const PrefService* prefs = profile()->GetPrefs();
  const bool is_icloud_drive_enabled = IsICloudDriveEnabled();
  const bool is_active_profile_authenticator_user =
      IsActiveProfileAuthenticatorUser(prefs);
  dialog_controller_->set_allow_icloud_keychain(
      request_source == RequestSource::kWebAuthentication);
  dialog_controller_->set_has_icloud_drive_enabled(is_icloud_drive_enabled);
  dialog_controller_->set_is_active_profile_authenticator_user(
      is_active_profile_authenticator_user);
  dialog_controller_->set_should_create_in_icloud_keychain(
      ShouldCreateInICloudKeychain(
          request_source, is_active_profile_authenticator_user,
          is_icloud_drive_enabled, rp_id == "google.com",
          GetICloudKeychainPref(prefs)));
}

#endif

void ChromeAuthenticatorRequestDelegate::OnPasswordCredentialsReceived(
    PasswordCredentials credentials) {
  pending_password_credentials_ =
      std::make_unique<PasswordCredentials>(std::move(credentials));
  TryToShowUI();
}

void ChromeAuthenticatorRequestDelegate::UpdateModelForTransportAvailability(
    const TransportAvailabilityInfo& tai) {
  dialog_model_->request_type = tai.request_type;
  dialog_model_->resident_key_requirement = tai.resident_key_requirement;
  dialog_model_->attestation_conveyance_preference =
      tai.attestation_conveyance_preference;
  dialog_model_->ble_adapter_is_powered =
      tai.ble_status == device::FidoRequestHandlerBase::BleStatus::kOn;
  dialog_model_->show_security_key_on_qr_sheet =
      base::Contains(tai.available_transports,
                     device::FidoTransportProtocol::kUsbHumanInterfaceDevice);
  dialog_model_->is_off_the_record = tai.is_off_the_record_context;
  dialog_model_->platform_has_biometrics = tai.platform_has_biometrics;
}
