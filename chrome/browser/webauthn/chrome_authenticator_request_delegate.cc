// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/web_authentication_proxy/web_authentication_proxy_service.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/webauthn/authenticator_request_dialog.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/cablev2_devices.h"
#include "chrome/browser/webauthn/webauthn_pref_names.h"
#include "chrome/browser/webauthn/webauthn_switches.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/device_event_log/device_event_log.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "crypto/random.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/features.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_discovery_factory.h"
#include "extensions/common/constants.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"

#if BUILDFLAG(IS_MAC)
#include "device/fido/mac/authenticator.h"
#include "device/fido/mac/credential_metadata.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "device/fido/win/authenticator.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/webauthn_request_registrar.h"
#include "ui/aura/window.h"
#endif

namespace {

ChromeAuthenticatorRequestDelegate::TestObserver* g_observer = nullptr;

// Returns true iff |relying_party_id| is listed in the
// SecurityKeyPermitAttestation policy.
bool IsWebauthnRPIDListedInEnterprisePolicy(
    content::BrowserContext* browser_context,
    const std::string& relying_party_id) {
  const Profile* profile = Profile::FromBrowserContext(browser_context);
  const PrefService* prefs = profile->GetPrefs();
  const base::Value* permit_attestation =
      prefs->GetList(prefs::kSecurityKeyPermitAttestation);
  return std::any_of(permit_attestation->GetListDeprecated().begin(),
                     permit_attestation->GetListDeprecated().end(),
                     [&relying_party_id](const base::Value& v) {
                       return v.GetString() == relying_party_id;
                     });
}

#if BUILDFLAG(IS_WIN)
// kWebAuthnLastOperationWasNativeAPI is a boolean preference that records
// whether the last successful operation used the Windows native API. If so
// then we'll try and jump directly to it next time.
const char kWebAuthnLastOperationWasNativeAPI[] =
    "webauthn.last_op_used_native_api";
#endif

#if BUILDFLAG(IS_MAC)
const char kWebAuthnTouchIdMetadataSecretPrefName[] =
    "webauthn.touchid.metadata_secret";
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
  Profile* profile_;
};

}  // namespace

// ---------------------------------------------------------------------
// ChromeWebAuthenticationDelegate
// ---------------------------------------------------------------------

ChromeWebAuthenticationDelegate::~ChromeWebAuthenticationDelegate() = default;

#if !BUILDFLAG(IS_ANDROID)

static bool IsAllowedGoogleCorpRemoteProxyingOrigin(
    content::BrowserContext* browser_context,
    const url::Origin& caller_origin) {
  if (!base::FeatureList::IsEnabled(
          device::kWebAuthnGoogleCorpRemoteDesktopClientPrivilege)) {
    return false;
  }

  const Profile* profile = Profile::FromBrowserContext(browser_context);
  const PrefService* prefs = profile->GetPrefs();
  const bool google_corp_remote_proxied_request_allowed =
      prefs->GetBoolean(webauthn::pref_names::kRemoteProxiedRequestsAllowed);
  if (!google_corp_remote_proxied_request_allowed) {
    return false;
  }

  // The Google-internal CRD origin. The policy explicitly does not cover
  // external instances of CRD.
  constexpr char kGoogleCorpCrdOrigin[] =
      "https://remotedesktop.corp.google.com";
  if (caller_origin == url::Origin::Create(GURL(kGoogleCorpCrdOrigin))) {
    return true;
  }

  const std::string cmdline_allowed_origin(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          webauthn::switches::kRemoteProxiedRequestsAllowedAdditionalOrigin));
  if (cmdline_allowed_origin.empty()) {
    return false;
  }

  return caller_origin == url::Origin::Create(GURL(cmdline_allowed_origin));
}

bool ChromeWebAuthenticationDelegate::
    OverrideCallerOriginAndRelyingPartyIdValidation(
        content::BrowserContext* browser_context,
        const url::Origin& caller_origin,
        const std::string& relying_party_id) {
  // Allow the Google-internal version of Chrome Remote Desktop to bypass RP ID
  // validation so that it can execute WebAuthn requests on behalf of a remote
  // host. This behavior is gated on an internal-only platform-level enterprise
  // policy with the hard-coded Google-internal CRD origin. An additional origin
  // fro development and testing can be supplied via a switch, but only if the
  // enterprise policy has been enabled.
  if (IsAllowedGoogleCorpRemoteProxyingOrigin(browser_context, caller_origin)) {
    // Any Relying Party ID is allowed.
    return true;
  }

  // Allow chrome-extensions:// origins to make WebAuthn requests.
  // `MaybeGetRelyingPartyId` will override the RP ID to use when processing
  // requests from extensions.
  return caller_origin.scheme() == extensions::kExtensionScheme &&
         caller_origin.host() == relying_party_id;
}

absl::optional<std::string>
ChromeWebAuthenticationDelegate::MaybeGetRelyingPartyIdOverride(
    const std::string& claimed_relying_party_id,
    const url::Origin& caller_origin) {
  // Don't override cryptotoken processing.
  constexpr char kCryptotokenOrigin[] =
      "chrome-extension://kmendfapggjehodndflmmgagdbamhnfd";
  if (caller_origin == url::Origin::Create(GURL(kCryptotokenOrigin))) {
    return absl::nullopt;
  }

  // Otherwise, allow extensions to use WebAuthn and map their origins
  // directly to RP IDs.
  if (caller_origin.scheme() == extensions::kExtensionScheme) {
    // `OverrideCallerOriginAndRelyingPartyIdValidation' ensures an extension
    // must only use the extension identifier as the RP ID, no flexibility is
    // permitted. When interacting with authenticators, however, we use the
    // whole origin to avoid collisions with the RP ID space for HTTPS origins.
    return caller_origin.Serialize();
  }

  return absl::nullopt;
}

bool ChromeWebAuthenticationDelegate::ShouldPermitIndividualAttestation(
    content::BrowserContext* browser_context,
    const std::string& relying_party_id) {
  constexpr char kGoogleCorpAppId[] =
      "https://www.gstatic.com/securitykey/a/google.com/origins.json";

  // If the RP ID is actually the Google corp App ID (because the request is
  // actually a U2F request originating from cryptotoken), or is listed in the
  // enterprise policy, signal that individual attestation is permitted.
  return relying_party_id == kGoogleCorpAppId ||
         IsWebauthnRPIDListedInEnterprisePolicy(browser_context,
                                                relying_party_id);
}

bool ChromeWebAuthenticationDelegate::SupportsResidentKeys(
    content::RenderFrameHost* render_frame_host) {
  return true;
}

bool ChromeWebAuthenticationDelegate::IsFocused(
    content::WebContents* web_contents) {
  return web_contents->GetVisibility() == content::Visibility::VISIBLE;
}

#if BUILDFLAG(IS_WIN)
void ChromeWebAuthenticationDelegate::OperationSucceeded(
    content::BrowserContext* browser_context,
    bool used_win_api) {
  // If a registration or assertion operation was successful, record whether the
  // Windows native API was used for it. If so we'll jump directly to the native
  // UI for the next operation.
  Profile* const profile = Profile::FromBrowserContext(browser_context);
  if (profile->IsOffTheRecord()) {
    return;
  }

  profile->GetPrefs()->SetBoolean(kWebAuthnLastOperationWasNativeAPI,
                                  used_win_api);
}
#endif

absl::optional<bool> ChromeWebAuthenticationDelegate::
    IsUserVerifyingPlatformAuthenticatorAvailableOverride(
        content::RenderFrameHost* render_frame_host) {
  // If the testing API is active, its override takes precedence.
  absl::optional<bool> testing_api_override =
      content::WebAuthenticationDelegate::
          IsUserVerifyingPlatformAuthenticatorAvailableOverride(
              render_frame_host);
  if (testing_api_override) {
    return *testing_api_override;
  }

#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/908622): Enable platform authenticators in Incognito on
  // Windows once the API allows triggering an adequate warning dialog.
  if (render_frame_host->GetBrowserContext()->IsOffTheRecord()) {
    return false;
  }
#endif

  // Chrome disables platform authenticators is Guest sessions. They may be
  // available (behind an additional interstitial) in Incognito mode.
  Profile* profile =
      Profile::FromBrowserContext(render_frame_host->GetBrowserContext());
  if (profile->IsGuestSession()) {
    return false;
  }

  return absl::nullopt;
}

content::WebAuthenticationRequestProxy*
ChromeWebAuthenticationDelegate::MaybeGetRequestProxy(
    content::BrowserContext* browser_context) {
  return extensions::WebAuthenticationProxyServiceFactory::GetForBrowserContext(
      browser_context);
}

#endif  // !IS_ANDROID

#if BUILDFLAG(IS_MAC)
// static
ChromeWebAuthenticationDelegate::TouchIdAuthenticatorConfig
ChromeWebAuthenticationDelegate::TouchIdAuthenticatorConfigForProfile(
    Profile* profile) {
  constexpr char kKeychainAccessGroup[] =
      MAC_TEAM_IDENTIFIER_STRING "." MAC_BUNDLE_IDENTIFIER_STRING ".webauthn";

  std::string metadata_secret =
      profile->GetPrefs()->GetString(kWebAuthnTouchIdMetadataSecretPrefName);
  if (metadata_secret.empty() ||
      !base::Base64Decode(metadata_secret, &metadata_secret)) {
    metadata_secret = device::fido::mac::GenerateCredentialMetadataSecret();
    profile->GetPrefs()->SetString(
        kWebAuthnTouchIdMetadataSecretPrefName,
        base::Base64Encode(base::as_bytes(base::make_span(metadata_secret))));
  }

  return TouchIdAuthenticatorConfig{
      .keychain_access_group = kKeychainAccessGroup,
      .metadata_secret = std::move(metadata_secret)};
}

absl::optional<ChromeWebAuthenticationDelegate::TouchIdAuthenticatorConfig>
ChromeWebAuthenticationDelegate::GetTouchIdAuthenticatorConfig(
    content::BrowserContext* browser_context) {
  return TouchIdAuthenticatorConfigForProfile(
      Profile::FromBrowserContext(browser_context));
}
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS_ASH)
content::WebAuthenticationDelegate::ChromeOSGenerateRequestIdCallback
ChromeWebAuthenticationDelegate::GetGenerateRequestIdCallback(
    content::RenderFrameHost* render_frame_host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  aura::Window* window =
      render_frame_host->GetNativeView()->GetToplevelWindow();
  return ash::WebAuthnRequestRegistrar::Get()->GetRegisterCallback(window);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// ---------------------------------------------------------------------
// ChromeAuthenticatorRequestDelegate
// ---------------------------------------------------------------------

// static
void ChromeAuthenticatorRequestDelegate::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
#if BUILDFLAG(IS_WIN)
  registry->RegisterBooleanPref(kWebAuthnLastOperationWasNativeAPI, false);
#endif
#if BUILDFLAG(IS_MAC)
  registry->RegisterStringPref(kWebAuthnTouchIdMetadataSecretPrefName,
                               std::string());
#endif
  cablev2::RegisterProfilePrefs(registry);
}

ChromeAuthenticatorRequestDelegate::ChromeAuthenticatorRequestDelegate(
    content::RenderFrameHost* render_frame_host)
    : render_frame_host_id_(render_frame_host->GetGlobalId()) {
  if (g_observer) {
    g_observer->Created(this);
  }
}

ChromeAuthenticatorRequestDelegate::~ChromeAuthenticatorRequestDelegate() {
  // Currently, completion of the request is indicated by //content destroying
  // this delegate.
  if (weak_dialog_model_) {
    weak_dialog_model_->OnRequestComplete();
  }

  // The dialog model may be destroyed after the OnRequestComplete call.
  if (weak_dialog_model_) {
    weak_dialog_model_->RemoveObserver(this);
    weak_dialog_model_ = nullptr;
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

void ChromeAuthenticatorRequestDelegate::SetRelyingPartyId(
    const std::string& rp_id) {
  transient_dialog_model_holder_ =
      std::make_unique<AuthenticatorRequestDialogModel>(rp_id);
  weak_dialog_model_ = transient_dialog_model_holder_.get();
}

bool ChromeAuthenticatorRequestDelegate::DoesBlockRequestOnFailure(
    InterestingFailureReason reason) {
  if (!IsWebAuthnUIEnabled())
    return false;
  if (!weak_dialog_model_)
    return false;

  switch (reason) {
    case InterestingFailureReason::kTimeout:
      weak_dialog_model_->OnRequestTimeout();
      break;
    case InterestingFailureReason::kKeyNotRegistered:
      weak_dialog_model_->OnActivatedKeyNotRegistered();
      break;
    case InterestingFailureReason::kKeyAlreadyRegistered:
      weak_dialog_model_->OnActivatedKeyAlreadyRegistered();
      break;
    case InterestingFailureReason::kSoftPINBlock:
      weak_dialog_model_->OnSoftPINBlock();
      break;
    case InterestingFailureReason::kHardPINBlock:
      weak_dialog_model_->OnHardPINBlock();
      break;
    case InterestingFailureReason::kAuthenticatorRemovedDuringPINEntry:
      weak_dialog_model_->OnAuthenticatorRemovedDuringPINEntry();
      break;
    case InterestingFailureReason::kAuthenticatorMissingResidentKeys:
      weak_dialog_model_->OnAuthenticatorMissingResidentKeys();
      break;
    case InterestingFailureReason::kAuthenticatorMissingUserVerification:
      weak_dialog_model_->OnAuthenticatorMissingUserVerification();
      break;
    case InterestingFailureReason::kAuthenticatorMissingLargeBlob:
      weak_dialog_model_->OnAuthenticatorMissingLargeBlob();
      break;
    case InterestingFailureReason::kNoCommonAlgorithms:
      weak_dialog_model_->OnNoCommonAlgorithms();
      break;
    case InterestingFailureReason::kStorageFull:
      weak_dialog_model_->OnAuthenticatorStorageFull();
      break;
    case InterestingFailureReason::kUserConsentDenied:
      weak_dialog_model_->OnUserConsentDenied();
      break;
    case InterestingFailureReason::kWinUserCancelled:
      return weak_dialog_model_->OnWinUserCancelled();
  }
  return true;
}

void ChromeAuthenticatorRequestDelegate::RegisterActionCallbacks(
    base::OnceClosure cancel_callback,
    base::RepeatingClosure start_over_callback,
    device::FidoRequestHandlerBase::RequestCallback request_callback,
    base::RepeatingClosure bluetooth_adapter_power_on_callback) {
  request_callback_ = request_callback;
  cancel_callback_ = std::move(cancel_callback);
  start_over_callback_ = std::move(start_over_callback);

  weak_dialog_model_->SetRequestCallback(request_callback);
  weak_dialog_model_->SetBluetoothAdapterPowerOnCallback(
      bluetooth_adapter_power_on_callback);
}

void ChromeAuthenticatorRequestDelegate::ShouldReturnAttestation(
    const std::string& relying_party_id,
    const device::FidoAuthenticator* authenticator,
    bool is_enterprise_attestation,
    base::OnceCallback<void(bool)> callback) {
  if (IsWebauthnRPIDListedInEnterprisePolicy(GetBrowserContext(),
                                             relying_party_id)) {
    // Enterprise attestations should have been approved already and not reach
    // this point.
    DCHECK(!is_enterprise_attestation);
    std::move(callback).Run(true);
    return;
  }

  // Cryptotoken displays its own attestation consent prompt.
  // AuthenticatorCommon does not invoke ShouldReturnAttestation() for those
  // requests.
  if (disable_ui_) {
    NOTREACHED();
    std::move(callback).Run(false);
    return;
  }

#if BUILDFLAG(IS_WIN)
  if (authenticator->IsWinNativeApiAuthenticator() &&
      static_cast<const device::WinWebAuthnApiAuthenticator*>(authenticator)
          ->ShowsPrivacyNotice()) {
    // The OS' native API includes an attestation prompt.
    std::move(callback).Run(true);
    return;
  }
#endif  // BUILDFLAG(IS_WIN)

  weak_dialog_model_->RequestAttestationPermission(is_enterprise_attestation,
                                                   std::move(callback));
}

void ChromeAuthenticatorRequestDelegate::ConfigureCable(
    const url::Origin& origin,
    device::FidoRequestType request_type,
    base::span<const device::CableDiscoveryData> pairings_from_extension,
    device::FidoDiscoveryFactory* discovery_factory) {
  phone_names_.clear();
  phone_public_keys_.clear();

  const bool cable_extension_permitted = ShouldPermitCableExtension(origin);

  std::vector<device::CableDiscoveryData> pairings;
  if (cable_extension_permitted) {
    pairings.insert(pairings.end(), pairings_from_extension.begin(),
                    pairings_from_extension.end());
  }
  const bool cable_extension_provided = !pairings.empty();
  const bool cablev2_extension_provided =
      std::any_of(pairings.begin(), pairings.end(),
                  [](const device::CableDiscoveryData& v) -> bool {
                    return v.version == device::CableDiscoveryData::Version::V2;
                  });

  std::vector<std::unique_ptr<device::cablev2::Pairing>> paired_phones;
  std::vector<AuthenticatorRequestDialogModel::PairedPhone>
      paired_phone_entries;
  base::RepeatingCallback<void(size_t)> contact_phone_callback;
  if ((!cable_extension_provided ||
       base::FeatureList::IsEnabled(device::kWebAuthCableExtensionAnywhere)) &&
      base::FeatureList::IsEnabled(device::kWebAuthCableSecondFactor)) {
    DCHECK(phone_names_.empty());
    DCHECK(phone_public_keys_.empty());

    std::unique_ptr<cablev2::KnownDevices> known_devices =
        cablev2::KnownDevices::FromProfile(
            Profile::FromBrowserContext(GetBrowserContext()));
    if (g_observer) {
      known_devices->synced_devices =
          g_observer->GetCablePairingsFromSyncedDevices();
    }
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
      for (size_t i = 0; i < paired_phones.size(); i++) {
        const auto& phone = paired_phones[i];
        paired_phone_entries.emplace_back(phone->name, i,
                                          phone->peer_public_key_x962);
        phone_names_.push_back(phone->name);
        phone_public_keys_.push_back(phone->peer_public_key_x962);
      }

      contact_phone_callback = discovery_factory->get_cable_contact_callback();
    }
  }
  const bool have_paired_phones = !paired_phones.empty();

  const bool non_extension_cablev2_enabled =
      (!cable_extension_permitted ||
       base::FeatureList::IsEnabled(device::kWebAuthCableExtensionAnywhere)) &&
      (have_paired_phones ||
       base::FeatureList::IsEnabled(device::kWebAuthPhoneSupport));

  const bool android_accessory_possible =
      base::FeatureList::IsEnabled(device::kWebAuthPhoneSupport) ||
      cablev2_extension_provided ||
      (!cable_extension_permitted &&
       base::FeatureList::IsEnabled(device::kWebAuthCableSecondFactor));

  absl::optional<std::array<uint8_t, device::cablev2::kQRKeySize>>
      qr_generator_key;
  absl::optional<std::string> qr_string;
  if (non_extension_cablev2_enabled || cablev2_extension_provided) {
    // A QR key is generated for all caBLEv2 cases but whether the QR code is
    // displayed is up to the UI.
    qr_generator_key.emplace();
    crypto::RandBytes(*qr_generator_key);
    qr_string = device::cablev2::qr::Encode(*qr_generator_key);

    auto linking_handler = std::make_unique<CableLinkingEventHandler>(
        Profile::FromBrowserContext(GetBrowserContext()));
    discovery_factory->set_cable_pairing_callback(
        base::BindRepeating(&CableLinkingEventHandler::OnNewCablePairing,
                            std::move(linking_handler)));
    discovery_factory->set_cable_invalidated_pairing_callback(
        base::BindRepeating(
            &ChromeAuthenticatorRequestDelegate::OnInvalidatedCablePairing,
            weak_ptr_factory_.GetWeakPtr()));
    discovery_factory->set_network_context(
        SystemNetworkContextManager::GetInstance()->GetContext());
  }

  if (android_accessory_possible) {
    mojo::Remote<device::mojom::UsbDeviceManager> usb_device_manager;
    content::GetDeviceService().BindUsbDeviceManager(
        usb_device_manager.BindNewPipeAndPassReceiver());
    discovery_factory->set_android_accessory_params(
        std::move(usb_device_manager),
        l10n_util::GetStringUTF8(IDS_WEBAUTHN_CABLEV2_AOA_REQUEST_DESCRIPTION));
  }

  if (cable_extension_provided || non_extension_cablev2_enabled) {
    absl::optional<bool> extension_is_v2;
    if (cable_extension_provided) {
      extension_is_v2 = cablev2_extension_provided;
    }
    weak_dialog_model_->set_cable_transport_info(
        extension_is_v2, std::move(paired_phone_entries),
        std::move(contact_phone_callback), qr_string);
    discovery_factory->set_cable_data(request_type, std::move(pairings),
                                      qr_generator_key,
                                      std::move(paired_phones));
  }
}

void ChromeAuthenticatorRequestDelegate::SelectAccount(
    std::vector<device::AuthenticatorGetAssertionResponse> responses,
    base::OnceCallback<void(device::AuthenticatorGetAssertionResponse)>
        callback) {
  if (disable_ui_) {
    // Cryptotoken requests should never reach account selection.
    NOTREACHED();
    std::move(cancel_callback_).Run();
    return;
  }

  if (!weak_dialog_model_) {
    std::move(cancel_callback_).Run();
    return;
  }

  weak_dialog_model_->SelectAccount(std::move(responses), std::move(callback));
}

void ChromeAuthenticatorRequestDelegate::DisableUI() {
  disable_ui_ = true;
}

bool ChromeAuthenticatorRequestDelegate::IsWebAuthnUIEnabled() {
  // The UI is fully disabled for the entire request duration if either:
  // 1) The request originates from cryptotoken. The UI may be hidden in other
  // circumstances (e.g. while showing the native Windows WebAuthn UI). But in
  // those cases the UI is still enabled and can be shown e.g. for an
  // attestation consent prompt.
  // 2) A specialized UI is replacing the default WebAuthn UI, such as
  // Secure Payment Confirmation or Autofill.
  return !disable_ui_;
}

void ChromeAuthenticatorRequestDelegate::SetConditionalRequest(
    bool is_conditional) {
  is_conditional_ = is_conditional;
}

void ChromeAuthenticatorRequestDelegate::OnTransportAvailabilityEnumerated(
    device::FidoRequestHandlerBase::TransportAvailabilityInfo data) {
  if (g_observer) {
    g_observer->OnTransportAvailabilityEnumerated(this, &data);
  }

  if (disable_ui_ || !transient_dialog_model_holder_) {
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(GetRenderFrameHost());

  bool last_used_native_api = false;
#if BUILDFLAG(IS_WIN)
  PrefService* const prefs =
      user_prefs::UserPrefs::Get(web_contents->GetBrowserContext());
  last_used_native_api = prefs->GetBoolean(kWebAuthnLastOperationWasNativeAPI);
#endif

  weak_dialog_model_->AddObserver(this);
  weak_dialog_model_->StartFlow(std::move(data), is_conditional_,
                                last_used_native_api);

  Browser* const browser = chrome::FindBrowserWithWebContents(web_contents);
  if (browser) {
    browser->window()->UpdatePageActionIcon(PageActionIconType::kWebAuthn);
  }

  ShowAuthenticatorRequestDialog(web_contents,
                                 std::move(transient_dialog_model_holder_));

  if (g_observer) {
    g_observer->UIShown(this);
  }
}

bool ChromeAuthenticatorRequestDelegate::EmbedderControlsAuthenticatorDispatch(
    const device::FidoAuthenticator& authenticator) {
  // Decide whether the //device/fido code should dispatch the current
  // request to an authenticator immediately after it has been
  // discovered, or whether the embedder/UI takes charge of that by
  // invoking its RequestCallback.
  auto transport = authenticator.AuthenticatorTransport();
  return (is_conditional_ || IsWebAuthnUIEnabled()) &&
         (!transport ||  // Windows
          *transport == device::FidoTransportProtocol::kInternal);
}

void ChromeAuthenticatorRequestDelegate::FidoAuthenticatorAdded(
    const device::FidoAuthenticator& authenticator) {
  if (!IsWebAuthnUIEnabled())
    return;

  if (!weak_dialog_model_)
    return;

  weak_dialog_model_->AddAuthenticator(authenticator);
}

void ChromeAuthenticatorRequestDelegate::FidoAuthenticatorRemoved(
    base::StringPiece authenticator_id) {
  if (!IsWebAuthnUIEnabled())
    return;

  if (!weak_dialog_model_)
    return;

  weak_dialog_model_->RemoveAuthenticator(authenticator_id);
}

void ChromeAuthenticatorRequestDelegate::BluetoothAdapterPowerChanged(
    bool is_powered_on) {
  if (!weak_dialog_model_)
    return;

  weak_dialog_model_->OnBluetoothPoweredStateChanged(is_powered_on);
}

bool ChromeAuthenticatorRequestDelegate::SupportsPIN() const {
  return true;
}

void ChromeAuthenticatorRequestDelegate::CollectPIN(
    CollectPINOptions options,
    base::OnceCallback<void(std::u16string)> provide_pin_cb) {
  if (!weak_dialog_model_)
    return;

  weak_dialog_model_->CollectPIN(options.reason, options.error,
                                 options.min_pin_length, options.attempts,
                                 std::move(provide_pin_cb));
}

void ChromeAuthenticatorRequestDelegate::StartBioEnrollment(
    base::OnceClosure next_callback) {
  if (!weak_dialog_model_)
    return;

  weak_dialog_model_->StartInlineBioEnrollment(std::move(next_callback));
}

void ChromeAuthenticatorRequestDelegate::OnSampleCollected(
    int bio_samples_remaining) {
  if (!weak_dialog_model_)
    return;

  weak_dialog_model_->OnSampleCollected(bio_samples_remaining);
}

void ChromeAuthenticatorRequestDelegate::FinishCollectToken() {
  if (!weak_dialog_model_)
    return;

  weak_dialog_model_->FinishCollectToken();
}

void ChromeAuthenticatorRequestDelegate::OnRetryUserVerification(int attempts) {
  if (!weak_dialog_model_)
    return;

  weak_dialog_model_->OnRetryUserVerification(attempts);
}

void ChromeAuthenticatorRequestDelegate::OnStartOver() {
  DCHECK(start_over_callback_);
  start_over_callback_.Run();
}

void ChromeAuthenticatorRequestDelegate::OnModelDestroyed(
    AuthenticatorRequestDialogModel* model) {
  DCHECK(weak_dialog_model_ && weak_dialog_model_ == model);
  weak_dialog_model_ = nullptr;
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
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (browser) {
    NavigateParams params(browser,
                          GURL("chrome://settings/securityKeys/phones"),
                          ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL);
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    Navigate(&params);
  }
}

raw_ptr<AuthenticatorRequestDialogModel>
ChromeAuthenticatorRequestDelegate::GetDialogModelForTesting() {
  return weak_dialog_model_;
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

bool ChromeAuthenticatorRequestDelegate::ShouldPermitCableExtension(
    const url::Origin& origin) {
  if (base::FeatureList::IsEnabled(device::kWebAuthCableExtensionAnywhere)) {
    return true;
  }

  // TODO(crbug.com/1052397): Revisit the macro expression once build flag
  // switch of lacros-chrome is complete. If updating this, also update
  // kWebAuthCableServerLink.
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)

  // caBLEv1 is disabled on these platforms. It never launched on them because
  // it causes problems in bluez. Rather than disabling caBLE completely, which
  // is what was done prior to Jan 2022, this `return` just disables caBLEv1
  // on these platforms.
  return false;

#else

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

#endif
}

void ChromeAuthenticatorRequestDelegate::OnInvalidatedCablePairing(
    size_t failed_contact_index) {
  PrefService* const prefs =
      Profile::FromBrowserContext(GetBrowserContext())->GetPrefs();

  // A pairing was reported to be invalid. Delete it unless it came from Sync,
  // in which case there's nothing to be done.
  cablev2::DeletePairingByPublicKey(
      prefs, phone_public_keys_.at(failed_contact_index));

  if (weak_dialog_model_) {
    // Contact the next phone with the same name, if any, given that no
    // notification has been sent.
    weak_dialog_model_->OnPhoneContactFailed(
        phone_names_.at(failed_contact_index));
  }
}
