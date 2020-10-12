// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"

#include <memory>

#include "ash/public/cpp/login_screen.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/authpolicy/authpolicy_helper.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/chromeos/certificate_provider/pin_dialog_manager.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/chromeos/login/lock_screen_utils.h"
#include "chrome/browser/chromeos/login/reauth_stats.h"
#include "chrome/browser/chromeos/login/saml/public_saml_url_fetcher.h"
#include "chrome/browser/chromeos/login/saml/saml_metric_utils.h"
#include "chrome/browser/chromeos/login/screens/network_error.h"
#include "chrome/browser/chromeos/login/signin_partition_manager.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_webui.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager_util.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_network_configuration_updater.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "chrome/browser/ui/webui/chromeos/login/cookie_waiter.h"
#include "chrome/browser/ui/webui/chromeos/login/enrollment_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/user_creation_screen_handler.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chromeos/components/security_token_pin/constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/constants/devicetype.h"
#include "chromeos/dbus/util/version_loader.h"
#include "chromeos/login/auth/challenge_response/cert_utils.h"
#include "chromeos/login/auth/cryptohome_key_constants.h"
#include "chromeos/login/auth/saml_password_attributes.h"
#include "chromeos/login/auth/user_context.h"
#include "chromeos/network/onc/certificate_scope.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/login/localized_values_builder.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "services/network/nss_temp_certs_cache_chromeos.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/ime/chromeos/input_method_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"

using content::BrowserThread;
namespace em = enterprise_management;

namespace chromeos {

namespace {

const char kAuthIframeParentName[] = "signin-frame";

const char kEndpointGen[] = "1.0";

const char kOAUTHCodeCookie[] = "oauth_code";
const char kGAPSCookie[] = "GAPS";

// Must be kept consistent with ChromeOSSamlApiUsed in enums.xml
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused
enum class ChromeOSSamlApiUsed {
  kNotSamlLogin = 0,
  kSamlApiUsed = 1,
  kSamlApiNotUsed = 2,
  kMaxValue = kSamlApiNotUsed,
};

void RecordAPILogin(bool is_third_party_idp, bool is_api_used) {
  ChromeOSSamlApiUsed login_type;
  if (!is_third_party_idp) {
    login_type = ChromeOSSamlApiUsed::kNotSamlLogin;
  } else if (is_api_used) {
    login_type = ChromeOSSamlApiUsed::kSamlApiUsed;
  } else {
    login_type = ChromeOSSamlApiUsed::kSamlApiNotUsed;
  }
  base::UmaHistogramEnumeration("ChromeOS.SAML.APILogin", login_type);
}

policy::DeviceMode GetDeviceMode() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return connector->GetDeviceMode();
}

GaiaScreenHandler::GaiaScreenMode GetGaiaScreenMode(const std::string& email,
                                                    bool use_offline) {
  if (use_offline)
    return GaiaScreenHandler::GAIA_SCREEN_MODE_OFFLINE;

  if (GetDeviceMode() == policy::DEVICE_MODE_ENTERPRISE_AD)
    return GaiaScreenHandler::GAIA_SCREEN_MODE_AD;

  int authentication_behavior = 0;
  CrosSettings::Get()->GetInteger(kLoginAuthenticationBehavior,
                                  &authentication_behavior);
  if (authentication_behavior ==
      em::LoginAuthenticationBehaviorProto::SAML_INTERSTITIAL) {
    if (email.empty())
      return GaiaScreenHandler::GAIA_SCREEN_MODE_SAML_INTERSTITIAL;

    // If there's a populated email, we must check first that this user is using
    // SAML in order to decide whether to show the interstitial page.
    const user_manager::User* user = user_manager::UserManager::Get()->FindUser(
        user_manager::known_user::GetAccountId(email, std::string() /* id */,
                                               AccountType::UNKNOWN));

    if (user && user->using_saml())
      return GaiaScreenHandler::GAIA_SCREEN_MODE_SAML_INTERSTITIAL;
  }

  return GaiaScreenHandler::GAIA_SCREEN_MODE_DEFAULT;
}

std::string GetEnterpriseDisplayDomain() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return connector->GetEnterpriseDisplayDomain();
}

std::string GetEnterpriseEnrollmentDomain() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return connector->GetEnterpriseEnrollmentDomain();
}

std::string GetRealm() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return connector->GetRealm();
}

std::string GetChromeType() {
  switch (chromeos::GetDeviceType()) {
    case chromeos::DeviceType::kChromebox:
      return "chromebox";
    case chromeos::DeviceType::kChromebase:
      return "chromebase";
    case chromeos::DeviceType::kChromebit:
      return "chromebit";
    case chromeos::DeviceType::kChromebook:
      return "chromebook";
    default:
      return "chromedevice";
  }
}

void UpdateAuthParams(base::DictionaryValue* params,
                      bool is_restrictive_proxy) {
  CrosSettings* cros_settings = CrosSettings::Get();
  bool allow_new_user = true;
  cros_settings->GetBoolean(kAccountsPrefAllowNewUser, &allow_new_user);

  // nosignup flow if new users are not allowed.
  if (!allow_new_user || is_restrictive_proxy)
    params->SetString("flow", "nosignup");
}

bool ShouldCheckUserTypeBeforeAllowing() {
  if (!chromeos::features::IsFamilyLinkOnSchoolDeviceEnabled())
    return false;

  CrosSettings* cros_settings = CrosSettings::Get();
  bool family_link_allowed = false;
  cros_settings->GetBoolean(kAccountsPrefFamilyLinkAccountsAllowed,
                            &family_link_allowed);

  return family_link_allowed;
}

void RecordSAMLScrapingVerificationResultInHistogram(bool success) {
  UMA_HISTOGRAM_BOOLEAN("ChromeOS.SAML.Scraping.VerificationResult", success);
}

void PushFrontIMIfNotExists(const std::string& input_method,
                            std::vector<std::string>* input_methods) {
  if (input_method.empty())
    return;

  if (!base::Contains(*input_methods, input_method))
    input_methods->insert(input_methods->begin(), input_method);
}

bool IsOnline(NetworkPortalDetector::CaptivePortalStatus status) {
  return status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE;
}

void GetVersionAndConsent(std::string* out_version, bool* out_consent) {
  *out_version = version_loader::GetVersion(version_loader::VERSION_SHORT);
  *out_consent = GoogleUpdateSettings::GetCollectStatsConsent();
}

user_manager::UserType GetUsertypeFromServicesString(
    const ::login::StringList& services) {
  bool is_child = false;
  const bool support_usm =
      base::FeatureList::IsEnabled(::features::kCrOSEnableUSMUserService);
  using KnownFlags = base::flat_set<std::string>;
  const KnownFlags known_flags =
      support_usm ? KnownFlags({"uca", "usm"}) : KnownFlags({"uca"});

  size_t i = 0;
  for (const std::string& item : services) {
    if (known_flags.find(item) != known_flags.end()) {
      is_child = true;
    }
    ++i;
  }

  return is_child ? user_manager::USER_TYPE_CHILD
                  : user_manager::USER_TYPE_REGULAR;
}

user_manager::UserType CalculateUserType(const AccountId& account_id) {
  if (user_manager::UserManager::Get()->IsSupervisedAccountId(account_id))
    return user_manager::USER_TYPE_SUPERVISED;

  if (account_id.GetAccountType() == AccountType::ACTIVE_DIRECTORY)
    return user_manager::USER_TYPE_ACTIVE_DIRECTORY;

  return user_manager::USER_TYPE_REGULAR;
}

std::string GetAdErrorMessage(authpolicy::ErrorType error) {
  switch (error) {
    case authpolicy::ERROR_NETWORK_PROBLEM:
      return l10n_util::GetStringUTF8(IDS_AD_AUTH_NETWORK_ERROR);
    case authpolicy::ERROR_KDC_DOES_NOT_SUPPORT_ENCRYPTION_TYPE:
      return l10n_util::GetStringUTF8(IDS_AD_AUTH_NOT_SUPPORTED_ENCRYPTION);
    default:
      DLOG(WARNING) << "Unhandled error code: " << error;
      return l10n_util::GetStringUTF8(IDS_AD_AUTH_UNKNOWN_ERROR);
  }
}

bool ExtractSamlPasswordAttributesEnabled() {
  return base::FeatureList::IsEnabled(::features::kInSessionPasswordChange);
}

PinDialogManager* GetLoginScreenPinDialogManager() {
  DCHECK(ProfileHelper::IsSigninProfileInitialized());
  CertificateProviderService* certificate_provider_service =
      CertificateProviderServiceFactory::GetForBrowserContext(
          ProfileHelper::GetSigninProfile());
  return certificate_provider_service->pin_dialog_manager();
}

base::Value MakeSecurityTokenPinDialogParameters(
    security_token_pin::CodeType code_type,
    bool enable_user_input,
    security_token_pin::ErrorLabel error_label,
    int attempts_left) {
  base::Value params(base::Value::Type::DICTIONARY);
  params.SetIntKey("codeType", static_cast<int>(code_type));
  params.SetBoolKey("enableUserInput", enable_user_input);
  params.SetIntKey("errorLabel", static_cast<int>(error_label));
  params.SetIntKey("attemptsLeft", attempts_left);
  return params;
}

}  // namespace

constexpr StaticOobeScreenId GaiaView::kScreenId;

// A class that's used to specify the way how Gaia should be loaded.
struct GaiaScreenHandler::GaiaContext {
  GaiaContext();

  // Forces Gaia to reload.
  bool force_reload = false;

  // Whether Gaia should be loaded in offline mode.
  bool use_offline = false;

  // Email of the current user.
  std::string email;

  // GAIA ID of the current user.
  std::string gaia_id;

  // GAPS cookie.
  std::string gaps_cookie;
};

GaiaScreenHandler::GaiaContext::GaiaContext() {}

GaiaScreenHandler::GaiaScreenHandler(
    JSCallsContainer* js_calls_container,
    CoreOobeView* core_oobe_view,
    const scoped_refptr<NetworkStateInformer>& network_state_informer)
    : BaseScreenHandler(kScreenId, js_calls_container),
      network_state_informer_(network_state_informer),
      core_oobe_view_(core_oobe_view) {
  DCHECK(network_state_informer_.get());
  set_user_acted_method_path("login.GaiaSigninScreen.userActed");
}

GaiaScreenHandler::~GaiaScreenHandler() {
  if (network_portal_detector::IsInitialized())
    network_portal_detector::GetInstance()->RemoveObserver(this);
  if (is_security_token_pin_enabled_)
    GetLoginScreenPinDialogManager()->RemovePinDialogHost(this);
}

void GaiaScreenHandler::MaybePreloadAuthExtension() {
  // We shall not have network portal detector initialized, which unnecessarily
  // polls captive portal checking URL if we don't need to load gaia. See
  // go/bad-portal for more context.
  if (!signin_screen_handler_->ShouldLoadGaia())
    return;

  VLOG(1) << "MaybePreloadAuthExtension";

  if (network_portal_detector::IsInitialized())
    network_portal_detector::GetInstance()->AddAndFireObserver(this);

  // If cookies clearing was initiated or |dns_clear_task_running_| then auth
  // extension showing has already been initiated and preloading is pointless.
  if (!gaia_silent_load_ && !cookies_cleared_ && !dns_clear_task_running_ &&
      network_state_informer_->state() == NetworkStateInformer::ONLINE) {
    gaia_silent_load_ = true;
    gaia_silent_load_network_ = network_state_informer_->network_path();
    LoadAuthExtension(true /* force */, false /* offline */);
  }
}

void GaiaScreenHandler::DisableRestrictiveProxyCheckForTest() {
  disable_restrictive_proxy_check_for_test_ = true;
}

void GaiaScreenHandler::LoadGaia(const GaiaContext& context) {
  // Start a new session with SigninPartitionManager, generating a unique
  // StoragePartition.
  login::SigninPartitionManager* signin_partition_manager =
      login::SigninPartitionManager::Factory::GetForBrowserContext(
          Profile::FromWebUI(web_ui()));

  auto partition_call = base::BindOnce(
      &login::SigninPartitionManager::StartSigninSession,
      base::Unretained(signin_partition_manager), web_ui()->GetWebContents(),
      base::BindOnce(&GaiaScreenHandler::LoadGaiaWithPartition,
                     weak_factory_.GetWeakPtr(), context));

  if (!context.email.empty()) {
    const AccountId account_id = GetAccountId(
        context.email, std::string() /* id */, AccountType::UNKNOWN);
    const user_manager::User* const user =
        user_manager::UserManager::Get()->FindUser(account_id);

    if (user && user->using_saml() &&
        user->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT) {
      public_saml_url_fetcher_ =
          std::make_unique<chromeos::PublicSamlUrlFetcher>(account_id);
      public_saml_url_fetcher_->Fetch(std::move(partition_call));
      return;
    }
  }
  public_saml_url_fetcher_.reset();
  std::move(partition_call).Run();
}

void GaiaScreenHandler::LoadGaiaWithPartition(
    const GaiaContext& context,
    const std::string& partition_name) {
  auto callback =
      base::BindOnce(&GaiaScreenHandler::OnSetCookieForLoadGaiaWithPartition,
                     weak_factory_.GetWeakPtr(), context, partition_name);
  if (context.gaps_cookie.empty()) {
    std::move(callback).Run(net::CookieAccessResult());
    return;
  }

  // When the network service is enabled the webRequest API doesn't allow
  // modification of the cookie header. So manually write the GAPS cookie into
  // the CookieManager.
  login::SigninPartitionManager* signin_partition_manager =
      login::SigninPartitionManager::Factory::GetForBrowserContext(
          Profile::FromWebUI(web_ui()));
  content::StoragePartition* partition =
      signin_partition_manager->GetCurrentStoragePartition();
  if (!partition)
    return;

  // Note: The CanonicalCookie created here is not Secure. This is fine because
  // it's being set into a different StoragePartition than the user's actual
  // profile. The SetCanonicalCookie call will succeed regardless of the scheme
  // of |gaia_url| since there are no scheme restrictions since the cookie is
  // not Secure, and there is no preexisting Secure cookie in the profile that
  // would preclude updating it insecurely. |gaia_url| is usually secure, and
  // only insecure in local testing.

  std::string gaps_cookie_value(kGAPSCookie);
  gaps_cookie_value += "=" + context.gaps_cookie;
  const GURL& gaia_url = GaiaUrls::GetInstance()->gaia_url();
  std::unique_ptr<net::CanonicalCookie> cc(net::CanonicalCookie::Create(
      gaia_url, gaps_cookie_value, base::Time::Now(),
      base::nullopt /* server_time */));

  const net::CookieOptions options = net::CookieOptions::MakeAllInclusive();
  partition->GetCookieManagerForBrowserProcess()->SetCanonicalCookie(
      *cc.get(), gaia_url, options, std::move(callback));
}

void GaiaScreenHandler::OnSetCookieForLoadGaiaWithPartition(
    const GaiaContext& context,
    const std::string& partition_name,
    net::CookieAccessResult result) {
  std::unique_ptr<std::string> version = std::make_unique<std::string>();
  std::unique_ptr<bool> consent = std::make_unique<bool>();
  base::OnceClosure get_version_and_consent =
      base::BindOnce(&GetVersionAndConsent, base::Unretained(version.get()),
                     base::Unretained(consent.get()));
  base::OnceClosure load_gaia = base::BindOnce(
      &GaiaScreenHandler::LoadGaiaWithPartitionAndVersionAndConsent,
      weak_factory_.GetWeakPtr(), context, partition_name,
      base::Owned(version.release()), base::Owned(consent.release()));
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      std::move(get_version_and_consent), std::move(load_gaia));
}

void GaiaScreenHandler::LoadGaiaWithPartitionAndVersionAndConsent(
    const GaiaContext& context,
    const std::string& partition_name,
    const std::string* platform_version,
    const bool* collect_stats_consent) {
  base::DictionaryValue params;

  params.SetBoolean("forceReload", context.force_reload);
  params.SetString("gaiaId", context.gaia_id);
  params.SetBoolean("readOnlyEmail", true);
  params.SetString("email", context.email);

  UpdateAuthParams(&params, IsRestrictiveProxy());

  screen_mode_ = GetGaiaScreenMode(context.email, context.use_offline);
  params.SetInteger("screenMode", screen_mode_);

  if (screen_mode_ == GAIA_SCREEN_MODE_AD && !authpolicy_login_helper_)
    authpolicy_login_helper_ = std::make_unique<AuthPolicyHelper>();

  if (screen_mode_ != GAIA_SCREEN_MODE_OFFLINE) {
    const std::string app_locale = g_browser_process->GetApplicationLocale();
    if (!app_locale.empty())
      params.SetString("hl", app_locale);
  }

  std::string realm(GetRealm());
  if (!realm.empty()) {
    params.SetString("realm", realm);
  }

  const std::string enterprise_display_domain(GetEnterpriseDisplayDomain());
  const std::string enterprise_enrollment_domain(
      GetEnterpriseEnrollmentDomain());
  if (!enterprise_display_domain.empty())
    params.SetString("enterpriseDisplayDomain", enterprise_display_domain);
  if (!enterprise_enrollment_domain.empty()) {
    params.SetString("enterpriseEnrollmentDomain",
                     enterprise_enrollment_domain);
  }
  params.SetBoolean("enterpriseManagedDevice",
                    g_browser_process->platform_part()
                        ->browser_policy_connector_chromeos()
                        ->IsEnterpriseManaged());
  const AccountId& owner_account_id =
      user_manager::UserManager::Get()->GetOwnerAccountId();
  params.SetBoolean("hasDeviceOwner", owner_account_id.is_valid());
  if (owner_account_id.is_valid() &&
      !::features::IsParentAccessCodeForOnlineLoginEnabled()) {
    // Some Autotest policy tests appear to wipe the user list in Local State
    // but preserve a policy file referencing an owner: https://crbug.com/850139
    const user_manager::User* owner_user =
        user_manager::UserManager::Get()->FindUser(owner_account_id);
    if (owner_user &&
        owner_user->GetType() == user_manager::UserType::USER_TYPE_CHILD) {
      params.SetString("obfuscatedOwnerId", owner_account_id.GetGaiaId());
    }
  }

  params.SetString("chromeType", GetChromeType());
  params.SetString("clientId",
                   GaiaUrls::GetInstance()->oauth2_chrome_client_id());
  params.SetString("clientVersion", version_info::GetVersionNumber());
  if (!platform_version->empty())
    params.SetString("platformVersion", *platform_version);
  params.SetString("releaseChannel", chrome::GetChannelName());
  params.SetString("endpointGen", kEndpointGen);

  std::string email_domain;
  if (CrosSettings::Get()->GetString(kAccountsPrefLoginScreenDomainAutoComplete,
                                     &email_domain) &&
      !email_domain.empty()) {
    params.SetString("emailDomain", email_domain);
  }

  params.SetString("gaiaUrl", GaiaUrls::GetInstance()->gaia_url().spec());
  switch (gaia_path_) {
    case GaiaPath::kDefault:
      // Use the default gaia signin path embedded/setup/v2/chromeos which is
      // set in authenticator.js
      break;
    case GaiaPath::kChildSignup:
      params.SetString("gaiaPath",
                       GaiaUrls::GetInstance()
                           ->embedded_setup_chromeos_kid_signup_url()
                           .path()
                           .substr(1));
      break;
    case GaiaPath::kChildSignin:
      params.SetString("gaiaPath",
                       GaiaUrls::GetInstance()
                           ->embedded_setup_chromeos_kid_signin_url()
                           .path()
                           .substr(1));
      break;
  }

  // We only send |chromeos_board| Gaia URL parameter if user has opted into
  // sending device statistics.
  if (*collect_stats_consent)
    params.SetString("lsbReleaseBoard", base::SysInfo::GetLsbReleaseBoard());

  params.SetString("webviewPartitionName", partition_name);
  signin_partition_name_ = partition_name;

  params.SetBoolean("extractSamlPasswordAttributes",
                    ExtractSamlPasswordAttributesEnabled());
  params.SetBoolean("enableGaiaActionButtons", true);

  if (public_saml_url_fetcher_) {
    params.SetBoolean("startsOnSamlPage", true);
    DCHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kPublicAccountsSamlAclUrl));
    std::string saml_acl_url =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kPublicAccountsSamlAclUrl);
    params.SetString("samlAclUrl", saml_acl_url);
    if (public_saml_url_fetcher_->FetchSucceeded()) {
      params.SetString("frameUrl", public_saml_url_fetcher_->GetRedirectUrl());
    } else {
      // TODO: make the string localized.
      std::string msg = "Failed to fetch the SAML redirect URL from the server";
      core_oobe_view_->ShowSignInError(
          0, msg, std::string(), HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT);
      return;
    }
  }
  public_saml_url_fetcher_.reset();

  was_security_token_pin_canceled_ = false;

  frame_state_ = FRAME_STATE_LOADING;
  CallJS("login.GaiaSigninScreen.loadAuthExtension", params);
}

void GaiaScreenHandler::ReloadGaia(bool force_reload) {
  if (frame_state_ == FRAME_STATE_LOADING && !force_reload) {
    VLOG(1) << "Skipping reloading of Gaia since gaia is loading.";
    return;
  }
  NetworkStateInformer::State state = network_state_informer_->state();
  if (state != NetworkStateInformer::ONLINE &&
      !signin_screen_handler_->proxy_auth_dialog_need_reload_) {
    VLOG(1) << "Skipping reloading of Gaia since network state="
            << NetworkStateInformer::StatusString(state);
    return;
  }

  signin_screen_handler_->proxy_auth_dialog_need_reload_ = false;
  VLOG(1) << "Reloading Gaia.";
  frame_state_ = FRAME_STATE_LOADING;
  LoadAuthExtension(force_reload, false /* offline */);
}

void GaiaScreenHandler::MonitorOfflineIdle(bool is_online) {
  CallJS("login.GaiaSigninScreen.monitorOfflineIdle", is_online);
}

void GaiaScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("guestSignin", IDS_BROWSE_WITHOUT_SIGNING_IN_HTML);
  builder->Add("backButton", IDS_ACCNAME_BACK);
  builder->Add("closeButton", IDS_CLOSE);
  builder->Add("allowlistErrorConsumer", IDS_LOGIN_ERROR_ALLOWLIST);
  builder->Add("allowlistErrorEnterprise",
               IDS_ENTERPRISE_LOGIN_ERROR_ALLOWLIST);
  builder->Add("allowlistErrorEnterpriseAndFamilyLink",
               IDS_ENTERPRISE_AND_FAMILY_LINK_LOGIN_ERROR_ALLOWLIST);
  builder->Add("tryAgainButton", IDS_ALLOWLIST_ERROR_TRY_AGAIN_BUTTON);
  builder->Add("learnMoreButton", IDS_LEARN_MORE);
  builder->Add("gaiaLoading", IDS_LOGIN_GAIA_LOADING_MESSAGE);

  // Strings used by the SAML fatal error dialog.
  builder->Add("fatalErrorMessageNoAccountDetails",
               IDS_LOGIN_FATAL_ERROR_NO_ACCOUNT_DETAILS);
  builder->Add("fatalErrorMessageNoPassword",
               IDS_LOGIN_FATAL_ERROR_NO_PASSWORD);
  builder->Add("fatalErrorMessageVerificationFailed",
               IDS_LOGIN_FATAL_ERROR_PASSWORD_VERIFICATION);
  builder->Add("fatalErrorMessageInsecureURL",
               IDS_LOGIN_FATAL_ERROR_TEXT_INSECURE_URL);
  builder->Add("fatalErrorDoneButton", IDS_DONE);
  builder->Add("fatalErrorTryAgainButton",
               IDS_LOGIN_FATAL_ERROR_TRY_AGAIN_BUTTON);

  builder->AddF("loginWelcomeMessage", IDS_LOGIN_WELCOME_MESSAGE,
                ui::GetChromeOSDeviceTypeResourceId());
  builder->Add("offlineLoginEmail", IDS_OFFLINE_LOGIN_EMAIL);
  builder->Add("offlineLoginPassword", IDS_OFFLINE_LOGIN_PASSWORD);
  builder->Add("offlineLoginInvalidEmail", IDS_OFFLINE_LOGIN_INVALID_EMAIL);
  builder->Add("offlineLoginInvalidPassword",
               IDS_OFFLINE_LOGIN_INVALID_PASSWORD);
  builder->Add("offlineLoginNextBtn", IDS_OFFLINE_LOGIN_NEXT_BUTTON_TEXT);
  builder->Add("offlineLoginForgotPasswordBtn",
               IDS_OFFLINE_LOGIN_FORGOT_PASSWORD_BUTTON_TEXT);
  builder->Add("offlineLoginForgotPasswordDlg",
               IDS_OFFLINE_LOGIN_FORGOT_PASSWORD_DIALOG_TEXT);
  builder->Add("offlineLoginCloseBtn", IDS_OFFLINE_LOGIN_CLOSE_BUTTON_TEXT);
  builder->Add("enterpriseInfoMessage", IDS_LOGIN_DEVICE_MANAGED_BY_NOTICE);
  builder->Add("samlInterstitialMessage", IDS_LOGIN_SAML_INTERSTITIAL_MESSAGE);
  builder->Add("samlInterstitialChangeAccountLink",
               IDS_LOGIN_SAML_INTERSTITIAL_CHANGE_ACCOUNT_LINK_TEXT);
  builder->Add("samlInterstitialNextBtn",
               IDS_LOGIN_SAML_INTERSTITIAL_NEXT_BUTTON_TEXT);

  builder->Add("adAuthWelcomeMessage", IDS_AD_DOMAIN_AUTH_WELCOME_MESSAGE);
  builder->Add("adAuthLoginUsername", IDS_AD_AUTH_LOGIN_USER);
  builder->Add("adLoginPassword", IDS_AD_LOGIN_PASSWORD);

  builder->Add("adPassChangeOldPasswordHint",
               IDS_AD_PASSWORD_CHANGE_OLD_PASSWORD_HINT);
  builder->Add("adPassChangeNewPasswordHint",
               IDS_AD_PASSWORD_CHANGE_NEW_PASSWORD_HINT);
  builder->Add("adPassChangeRepeatNewPasswordHint",
               IDS_AD_PASSWORD_CHANGE_REPEAT_NEW_PASSWORD_HINT);
  builder->Add("adPassChangeOldPasswordError",
               IDS_AD_PASSWORD_CHANGE_INVALID_PASSWORD_ERROR);
  builder->Add("adPassChangeNewPasswordRejected",
               IDS_AD_PASSWORD_CHANGE_NEW_PASSWORD_REJECTED_SHORT_ERROR);
  builder->Add("adPassChangePasswordsMismatch",
               IDS_AD_PASSWORD_CHANGE_PASSWORDS_MISMATCH_ERROR);

  builder->Add("securityTokenPinDialogTitle",
               IDS_SAML_SECURITY_TOKEN_PIN_DIALOG_TITLE);
  builder->Add("securityTokenPinDialogSubtitle",
               IDS_SAML_SECURITY_TOKEN_PIN_DIALOG_SUBTITLE);
  builder->Add("securityTokenPinDialogTryAgain",
               IDS_SAML_SECURITY_TOKEN_PIN_DIALOG_TRY_AGAIN);
  builder->Add("securityTokenPinDialogAttemptsLeft",
               IDS_REQUEST_PIN_DIALOG_ATTEMPTS_LEFT);
  builder->Add("securityTokenPinDialogErrorAttempts",
               IDS_REQUEST_PIN_DIALOG_ERROR_ATTEMPTS);
  builder->Add("securityTokenPinDialogUnknownError",
               IDS_REQUEST_PIN_DIALOG_UNKNOWN_ERROR);
  builder->Add("securityTokenPinDialogUnknownInvalidPin",
               IDS_REQUEST_PIN_DIALOG_INVALID_PIN_ERROR);
  builder->Add("securityTokenPinDialogUnknownInvalidPuk",
               IDS_REQUEST_PIN_DIALOG_INVALID_PUK_ERROR);
  builder->Add("securityTokenPinDialogUnknownMaxAttemptsExceeded",
               IDS_REQUEST_PIN_DIALOG_MAX_ATTEMPTS_EXCEEDED_ERROR);
}

void GaiaScreenHandler::Initialize() {
  initialized_ = true;
  // This should be called only once on page load.
  AllowJavascript();
  if (show_on_init_) {
    show_on_init_ = false;
    ShowGaiaScreenIfReady();
  }
}

void GaiaScreenHandler::RegisterMessages() {
  AddCallback("webviewLoadAborted",
              &GaiaScreenHandler::HandleWebviewLoadAborted);
  AddCallback("completeLogin", &GaiaScreenHandler::HandleCompleteLogin);
  AddCallback("completeAuthentication",
              &GaiaScreenHandler::HandleCompleteAuthentication);
  AddCallback("usingSAMLAPI", &GaiaScreenHandler::HandleUsingSAMLAPI);
  AddCallback("recordSAMLProvider",
              &GaiaScreenHandler::HandleRecordSAMLProvider);
  AddCallback("scrapedPasswordCount",
              &GaiaScreenHandler::HandleScrapedPasswordCount);
  AddCallback("scrapedPasswordVerificationFailed",
              &GaiaScreenHandler::HandleScrapedPasswordVerificationFailed);
  AddCallback("samlChallengeMachineKey",
              &GaiaScreenHandler::HandleSamlChallengeMachineKey);
  AddCallback("loginWebuiReady", &GaiaScreenHandler::HandleGaiaUIReady);
  AddCallback("identifierEntered", &GaiaScreenHandler::HandleIdentifierEntered);
  AddCallback("updateOfflineLogin",
              &GaiaScreenHandler::SetOfflineLoginIsActive);
  AddCallback("authExtensionLoaded",
              &GaiaScreenHandler::HandleAuthExtensionLoaded);
  AddCallback("completeAdAuthentication",
              &GaiaScreenHandler::HandleCompleteAdAuthentication);
  AddCallback("cancelAdAuthentication",
              &GaiaScreenHandler::HandleCancelActiveDirectoryAuth);
  AddRawCallback("showAddUser", &GaiaScreenHandler::HandleShowAddUser);
  AddCallback("getIsSamlUserPasswordless",
              &GaiaScreenHandler::HandleGetIsSamlUserPasswordless);
  AddCallback("showGuestInOobe", &GaiaScreenHandler::HandleShowGuestInOobe);
  AddCallback("samlStateChanged", &GaiaScreenHandler::HandleSamlStateChanged);
  AddCallback("securityTokenPinEntered",
              &GaiaScreenHandler::HandleSecurityTokenPinEntered);

  // Allow UMA metrics collection from JS.
  web_ui()->AddMessageHandler(std::make_unique<MetricsHandler>());
  BaseScreenHandler::RegisterMessages();
}

void GaiaScreenHandler::OnPortalDetectionCompleted(
    const NetworkState* network,
    const NetworkPortalDetector::CaptivePortalState& state) {
  VLOG(1) << "OnPortalDetectionCompleted "
          << NetworkPortalDetector::CaptivePortalStatusString(state.status);

  const NetworkPortalDetector::CaptivePortalStatus previous_status =
      captive_portal_status_;
  captive_portal_status_ = state.status;
  if (IsOfflineLoginActive() ||
      IsOnline(captive_portal_status_) == IsOnline(previous_status) ||
      disable_restrictive_proxy_check_for_test_ ||
      GetCurrentScreen() != kScreenId)
    return;

  LoadAuthExtension(true /* force */, false /* offline */);
}

void GaiaScreenHandler::HandleIdentifierEntered(const std::string& user_email) {
  // We cannot tell a user type from the identifier, so we delay checking if
  // the account should be allowed.
  if (ShouldCheckUserTypeBeforeAllowing())
    return;

  if (LoginDisplayHost::default_host() &&
      !LoginDisplayHost::default_host()->IsUserAllowlisted(
          user_manager::known_user::GetAccountId(
              user_email, std::string() /* id */, AccountType::UNKNOWN),
          base::nullopt)) {
    ShowAllowlistCheckFailedError();
  }
}

void GaiaScreenHandler::HandleAuthExtensionLoaded() {
  VLOG(1) << "Auth extension finished loading";
  auth_extension_being_loaded_ = false;
  // Recreate the client cert usage observer, in order to track only the certs
  // used during the current sign-in attempt.
  extension_provided_client_cert_usage_observer_ =
      std::make_unique<LoginClientCertUsageObserver>();
}

void GaiaScreenHandler::HandleWebviewLoadAborted(int error_code) {
  if (error_code == net::ERR_INVALID_AUTH_CREDENTIALS) {
    // Silently ignore this error - it is used as an intermediate state for
    // committed interstitials (see https://crbug.com/1049349 for details).
    return;
  }
  if (error_code == net::ERR_ABORTED) {
    LOG(WARNING) << "Ignoring Gaia webview error: "
                 << net::ErrorToShortString(error_code);
    return;
  }
  if ((was_security_token_pin_canceled_ && error_code == net::ERR_FAILED) ||
      (is_security_token_pin_dialog_running() &&
       error_code == net::ERR_TIMED_OUT)) {
    // Specific errors are expected when the security token PIN is aborted
    // (either with a generic failure if the user canceled the dialog, or with a
    // timeout error if the user didn't enter it on time). In that case, return
    // the user back to the first sign-in step instead of showing the network
    // error screen.
    ReloadGaia(/*force_reload=*/true);
    return;
  }

  frame_error_ = static_cast<net::Error>(error_code);
  LOG(ERROR) << "Gaia webview error: " << net::ErrorToShortString(error_code);
  NetworkError::ErrorReason error_reason =
      NetworkError::ERROR_REASON_FRAME_ERROR;
  frame_state_ = FRAME_STATE_ERROR;
  UpdateState(error_reason);
}

AccountId GaiaScreenHandler::GetAccountId(
    const std::string& authenticated_email,
    const std::string& id,
    const AccountType& account_type) const {
  const std::string canonicalized_email =
      gaia::CanonicalizeEmail(gaia::SanitizeEmail(authenticated_email));

  const AccountId account_id = user_manager::known_user::GetAccountId(
      authenticated_email, id, account_type);

  if (account_id.GetUserEmail() != canonicalized_email) {
    LOG(WARNING) << "Existing user '" << account_id.GetUserEmail()
                 << "' authenticated by alias '" << canonicalized_email << "'.";
  }

  return account_id;
}

void GaiaScreenHandler::DoAdAuth(
    const std::string& username,
    const Key& key,
    authpolicy::ErrorType error,
    const authpolicy::ActiveDirectoryAccountInfo& account_info) {
  if (error != authpolicy::ERROR_NONE)
    authpolicy_login_helper_->CancelRequestsAndRestart();

  switch (error) {
    case authpolicy::ERROR_NONE: {
      DCHECK(account_info.has_account_id() &&
             !account_info.account_id().empty() &&
             LoginDisplayHost::default_host());
      const AccountId account_id(GetAccountId(
          username, account_info.account_id(), AccountType::ACTIVE_DIRECTORY));
      LoginDisplayHost::default_host()->SetDisplayAndGivenName(
          account_info.display_name(), account_info.given_name());
      UserContext user_context(
          user_manager::UserType::USER_TYPE_ACTIVE_DIRECTORY, account_id);
      user_context.SetKey(key);
      user_context.SetAuthFlow(UserContext::AUTH_FLOW_ACTIVE_DIRECTORY);
      user_context.SetIsUsingOAuth(false);
      LoginDisplayHost::default_host()->CompleteLogin(user_context);
      break;
    }
    case authpolicy::ERROR_PASSWORD_EXPIRED:
      LoginDisplayHost::default_host()
          ->GetWizardController()
          ->ShowActiveDirectoryPasswordChangeScreen(username);
      break;
    case authpolicy::ERROR_PARSE_UPN_FAILED:
    case authpolicy::ERROR_BAD_USER_NAME:
      CallJS("login.GaiaSigninScreen.invalidateAd", username,
             static_cast<int>(ActiveDirectoryErrorState::BAD_USERNAME));
      break;
    case authpolicy::ERROR_BAD_PASSWORD:
      CallJS("login.GaiaSigninScreen.invalidateAd", username,
             static_cast<int>(ActiveDirectoryErrorState::BAD_AUTH_PASSWORD));
      break;
    default:
      CallJS("login.GaiaSigninScreen.invalidateAd", username,
             static_cast<int>(ActiveDirectoryErrorState::NONE));
      core_oobe_view_->ShowSignInError(
          0, GetAdErrorMessage(error), std::string(),
          HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT);
  }
}

void GaiaScreenHandler::HandleCompleteAdAuthentication(
    const std::string& username,
    const std::string& password) {
  if (LoginDisplayHost::default_host())
    LoginDisplayHost::default_host()->SetDisplayEmail(username);

  populated_account_id_ = AccountId::FromUserEmail(username);
  DCHECK(authpolicy_login_helper_);
  Key key(password);
  key.SetLabel(kCryptohomeGaiaKeyLabel);
  authpolicy_login_helper_->AuthenticateUser(
      username, std::string() /* object_guid */, password,
      base::BindOnce(&GaiaScreenHandler::DoAdAuth, weak_factory_.GetWeakPtr(),
                     username, key));
}

void GaiaScreenHandler::HandleCancelActiveDirectoryAuth() {
  DCHECK(authpolicy_login_helper_);
  authpolicy_login_helper_->CancelRequestsAndRestart();
}

void GaiaScreenHandler::HandleCompleteAuthentication(
    const std::string& gaia_id,
    const std::string& email,
    const std::string& password,
    bool using_saml,
    const ::login::StringList& services,
    const base::DictionaryValue* password_attributes) {
  if (!LoginDisplayHost::default_host())
    return;

  DCHECK(!email.empty());
  DCHECK(!gaia_id.empty());

  // Execute delayed allowlist check that is based on user type.
  const user_manager::UserType user_type =
      GetUsertypeFromServicesString(services);
  if (ShouldCheckUserTypeBeforeAllowing() &&
      !LoginDisplayHost::default_host()->IsUserAllowlisted(
          GetAccountId(email, gaia_id, AccountType::GOOGLE), user_type)) {
    ShowAllowlistCheckFailedError();
    return;
  }

  const std::string sanitized_email = gaia::SanitizeEmail(email);
  LoginDisplayHost::default_host()->SetDisplayEmail(sanitized_email);

  pending_user_context_ = std::make_unique<UserContext>();
  std::string error_message;
  if (!BuildUserContextForGaiaSignIn(
          GetUsertypeFromServicesString(services),
          GetAccountId(email, gaia_id, AccountType::GOOGLE), using_saml,
          password, SamlPasswordAttributes::FromJs(*password_attributes),
          pending_user_context_.get(), &error_message)) {
    core_oobe_view_->ShowSignInError(0, error_message, std::string(),
                                     HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT);
    pending_user_context_.reset();
    return;
  }

  ContinueAuthenticationWhenCookiesAvailable();

  if (test_expects_complete_login_) {
    VLOG(2) << "Complete test login for " << sanitized_email
            << ", requested=" << test_user_;

    test_expects_complete_login_ = false;
    test_user_.clear();
    test_pass_.clear();
  }
}

void GaiaScreenHandler::ContinueAuthenticationWhenCookiesAvailable() {
  // When the network service is enabled, the webRequest API doesn't expose
  // cookie headers. So manually fetch the cookies for the GAIA URL from the
  // CookieManager.
  login::SigninPartitionManager* signin_partition_manager =
      login::SigninPartitionManager::Factory::GetForBrowserContext(
          Profile::FromWebUI(web_ui()));
  content::StoragePartition* partition =
      signin_partition_manager->GetCurrentStoragePartition();
  if (!partition)
    return;

  // Validity check that partition did not change during login flow.
  DCHECK_EQ(signin_partition_manager->GetCurrentStoragePartitionName(),
            signin_partition_name_);

  network::mojom::CookieManager* cookie_manager =
      partition->GetCookieManagerForBrowserProcess();
  if (!oauth_code_waiter_) {
    // Set listener before requesting the cookies to avoid race conditions.
    oauth_code_waiter_ = std::make_unique<CookieWaiter>(
        cookie_manager, kOAUTHCodeCookie,
        base::BindRepeating(
            &GaiaScreenHandler::ContinueAuthenticationWhenCookiesAvailable,
            weak_factory_.GetWeakPtr()),
        base::BindOnce(&GaiaScreenHandler::OnCookieWaitTimeout,
                       weak_factory_.GetWeakPtr()));
  }

  const net::CookieOptions cookie_options =
      net::CookieOptions::MakeAllInclusive();
  cookie_manager->GetCookieList(
      GaiaUrls::GetInstance()->gaia_url(), cookie_options,
      base::BindOnce(&GaiaScreenHandler::OnGetCookiesForCompleteAuthentication,
                     weak_factory_.GetWeakPtr()));
}

void GaiaScreenHandler::OnGetCookiesForCompleteAuthentication(
    const net::CookieAccessResultList& cookies,
    const net::CookieAccessResultList& excluded_cookies) {
  std::string auth_code, gaps_cookie;
  for (const auto& cookie_with_access_result : cookies) {
    const auto& cookie = cookie_with_access_result.cookie;
    if (cookie.Name() == kOAUTHCodeCookie)
      auth_code = cookie.Value();
    else if (cookie.Name() == kGAPSCookie)
      gaps_cookie = cookie.Value();
  }

  if (auth_code.empty()) {
    // Will try again from oauth_code_waiter callback.
    return;
  }

  DCHECK(pending_user_context_);
  UserContext user_context = *pending_user_context_;
  pending_user_context_.reset();
  oauth_code_waiter_.reset();

  user_context.SetAuthCode(auth_code);
  if (!gaps_cookie.empty())
    user_context.SetGAPSCookie(gaps_cookie);

  LoginDisplayHost::default_host()->CompleteLogin(user_context);
}

void GaiaScreenHandler::OnCookieWaitTimeout() {
  DCHECK(pending_user_context_);
  pending_user_context_.reset();
  oauth_code_waiter_.reset();
  LoadAuthExtension(true /* force */, false /* offline */);
  core_oobe_view_->ShowSignInError(
      0, l10n_util::GetStringUTF8(IDS_LOGIN_FATAL_ERROR_NO_AUTH_TOKEN),
      std::string(), HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT);
}

void GaiaScreenHandler::HandleCompleteLogin(const std::string& gaia_id,
                                            const std::string& typed_email,
                                            const std::string& password,
                                            bool using_saml) {
  VLOG(1) << "HandleCompleteLogin";
  DoCompleteLogin(gaia_id, typed_email, password, using_saml,
                  SamlPasswordAttributes());
}

void GaiaScreenHandler::HandleUsingSAMLAPI(bool is_third_party_idp) {
  SetSAMLPrincipalsAPIUsed(is_third_party_idp, /*is_api_used=*/true);
}

void GaiaScreenHandler::HandleRecordSAMLProvider(
    const std::string& x509certificate) {
  metrics::RecordSAMLProvider(x509certificate);
}

void GaiaScreenHandler::HandleScrapedPasswordCount(int password_count) {
  // We are handling scraped passwords here so this is SAML flow without
  // Chrome Credentials Passing API
  SetSAMLPrincipalsAPIUsed(/*is_third_party_idp=*/true, /*is_api_used=*/false);
  // Use a histogram that has 11 buckets, one for each of the values in [0, 9]
  // and an overflow bucket at the end.
  UMA_HISTOGRAM_ENUMERATION("ChromeOS.SAML.Scraping.PasswordCountAll",
                            std::min(password_count, 10), 11);
  if (password_count == 0)
    HandleScrapedPasswordVerificationFailed();
}

void GaiaScreenHandler::HandleScrapedPasswordVerificationFailed() {
  RecordSAMLScrapingVerificationResultInHistogram(false);
}

void GaiaScreenHandler::HandleSamlChallengeMachineKey(
    const std::string& callback_id,
    const std::string& url,
    const std::string& challenge) {
  CreateSamlChallengeKeyHandler();
  saml_challenge_key_handler_->Run(
      Profile::FromWebUI(web_ui()),
      base::BindOnce(&GaiaScreenHandler::ResolveJavascriptCallback,
                     weak_factory_.GetWeakPtr(), base::Value(callback_id)),
      GURL(url), challenge);
}

void GaiaScreenHandler::HandleGaiaUIReady() {
  VLOG(1) << "Gaia is loaded";

  // As we could miss and window.onload could already be called, restore
  // focus to current pod (see crbug/175243).
  if (gaia_silent_load_)
    signin_screen_handler_->RefocusCurrentPod();

  frame_error_ = net::OK;
  frame_state_ = FRAME_STATE_LOADED;

  if (network_state_informer_->state() == NetworkStateInformer::ONLINE)
    UpdateState(NetworkError::ERROR_REASON_UPDATE);

  if (test_expects_complete_login_)
    SubmitLoginFormForTest();

  if (LoginDisplayHost::default_host()) {
    LoginDisplayHost::default_host()->OnGaiaScreenReady();
  } else {
    // Used to debug crbug.com/902315. Feel free to remove after that is fixed.
    LOG(ERROR) << "HandleGaiaUIReady: There is no LoginDisplayHost";
  }
}

void GaiaScreenHandler::HandleShowAddUser(const base::ListValue* args) {
  // TODO(xiaoyinh): Add trace event for gaia webui in views login screen.
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT0("ui", "ShowAddUser",
                                      LoginDisplayHostWebUI::kShowLoginWebUIid);

  std::string email;
  // |args| can be null if it's OOBE.
  if (args)
    args->GetString(0, &email);
  populated_account_id_ = AccountId::FromUserEmail(email);
  OnShowAddUser();
}

void GaiaScreenHandler::HandleGetIsSamlUserPasswordless(
    const std::string& callback_id,
    const std::string& typed_email,
    const std::string& gaia_id) {
  const bool is_saml_user_passwordless =
      extension_provided_client_cert_usage_observer_ &&
      extension_provided_client_cert_usage_observer_->ClientCertsWereUsed();
  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(is_saml_user_passwordless));
}

void GaiaScreenHandler::HandleShowGuestInOobe(bool show) {
  ash::LoginScreen::Get()->ShowGuestButtonInOobe(show);
}

void GaiaScreenHandler::HandleSamlStateChanged(bool is_saml) {
  if (is_saml == is_security_token_pin_enabled_) {
    // We're already in the needed |is_security_token_pin_enabled_| state.
    return;
  }
  // Enable ourselves as a security token PIN dialog host during the SAML
  // sign-in, so that when the SAML page requires client authentication (e.g.,
  // against a smart card), this PIN request is embedded into the SAML login UI.
  if (is_saml) {
    GetLoginScreenPinDialogManager()->AddPinDialogHost(this);
  } else {
    security_token_pin_entered_callback_.Reset();
    security_token_pin_dialog_closed_callback_.Reset();
    GetLoginScreenPinDialogManager()->RemovePinDialogHost(this);
  }
  is_security_token_pin_enabled_ = is_saml;
}

void GaiaScreenHandler::HandleSecurityTokenPinEntered(
    const std::string& user_input) {
  // Invariant: when the pin_entered_callback is present, the closed_callback
  // must be present as well.
  DCHECK(!security_token_pin_entered_callback_ ||
         security_token_pin_dialog_closed_callback_);

  if (!is_security_token_pin_dialog_running()) {
    // The PIN request has already been canceled on the handler side.
    return;
  }

  was_security_token_pin_canceled_ = user_input.empty();
  if (user_input.empty()) {
    security_token_pin_entered_callback_.Reset();
    std::move(security_token_pin_dialog_closed_callback_).Run();
  } else {
    // The callback must be non-null, since the UI implementation should not
    // send multiple non-empty results.
    std::move(security_token_pin_entered_callback_).Run(user_input);
    // Keep |security_token_pin_dialog_closed_callback_|, in order to be able to
    // notify about the dialog closing afterwards.
  }
}

void GaiaScreenHandler::OnShowAddUser() {
  signin_screen_handler_->is_account_picker_showing_first_time_ = false;
  lock_screen_utils::EnforceDevicePolicyInputMethods(std::string());
  LoadGaiaAsync(EmptyAccountId());
  LoginDisplayHost::default_host()->StartWizard(
      chromeos::features::IsChildSpecificSigninEnabled()
          ? UserCreationView::kScreenId
          : GaiaView::kScreenId);
}

void GaiaScreenHandler::DoCompleteLogin(
    const std::string& gaia_id,
    const std::string& typed_email,
    const std::string& password,
    bool using_saml,
    const SamlPasswordAttributes& password_attributes) {
  if (using_saml && !using_saml_api_)
    RecordSAMLScrapingVerificationResultInHistogram(true);

  DCHECK(!typed_email.empty());
  DCHECK(!gaia_id.empty());
  const std::string sanitized_email = gaia::SanitizeEmail(typed_email);
  LoginDisplayHost::default_host()->SetDisplayEmail(sanitized_email);
  const AccountId account_id =
      GetAccountId(typed_email, gaia_id, AccountType::GOOGLE);
  const user_manager::User* const user =
      user_manager::UserManager::Get()->FindUser(account_id);

  UserContext user_context;
  std::string error_message;
  if (!BuildUserContextForGaiaSignIn(
          user ? user->GetType() : CalculateUserType(account_id),
          GetAccountId(typed_email, gaia_id, AccountType::GOOGLE), using_saml,
          password, password_attributes, &user_context, &error_message)) {
    core_oobe_view_->ShowSignInError(0, error_message, std::string(),
                                     HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT);
    return;
  }

  LoginDisplayHost::default_host()->CompleteLogin(user_context);
}

void GaiaScreenHandler::StartClearingDnsCache() {
  if (dns_clear_task_running_ ||
      !g_browser_process->system_network_context_manager()) {
    return;
  }

  dns_cleared_ = false;

  g_browser_process->system_network_context_manager()
      ->GetContext()
      ->ClearHostCache(nullptr /* filter */,
                       // Need to ensure that even if the network service
                       // crashes, OnDnsCleared() is invoked.
                       mojo::WrapCallbackWithDropHandler(
                           base::BindOnce(&GaiaScreenHandler::OnDnsCleared,
                                          weak_factory_.GetWeakPtr()),
                           base::BindOnce(&GaiaScreenHandler::OnDnsCleared,
                                          weak_factory_.GetWeakPtr())));
  dns_clear_task_running_ = true;
}

void GaiaScreenHandler::OnDnsCleared() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  dns_clear_task_running_ = false;
  dns_cleared_ = true;
  ShowGaiaScreenIfReady();
}

void GaiaScreenHandler::StartClearingCookies(
    const base::Closure& on_clear_callback) {
  cookies_cleared_ = false;
  ProfileHelper* profile_helper = ProfileHelper::Get();
  LOG_ASSERT(Profile::FromWebUI(web_ui()) ==
             profile_helper->GetSigninProfile());
  profile_helper->ClearSigninProfile(
      base::Bind(&GaiaScreenHandler::OnCookiesCleared,
                 weak_factory_.GetWeakPtr(), on_clear_callback));
}

void GaiaScreenHandler::OnCookiesCleared(
    const base::Closure& on_clear_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  cookies_cleared_ = true;
  on_clear_callback.Run();
}

void GaiaScreenHandler::SubmitLoginFormForTest() {
  VLOG(2) << "Submit login form for test, user=" << test_user_;

  content::RenderFrameHost* frame =
      signin::GetAuthFrame(web_ui()->GetWebContents(), kAuthIframeParentName);

  // clang-format off
  std::string code =
      "document.getElementById('identifier').value = '" + test_user_ + "';"
      "document.getElementById('nextButton').click();";
  // clang-format on

  frame->ExecuteJavaScriptForTests(base::ASCIIToUTF16(code),
                                   base::NullCallback());

  if (!test_services_.empty()) {
    // Prefix each doublequote with backslash, so that it will remain correct
    // JSON after assigning to the element property.
    std::string escaped_services;
    base::ReplaceChars(test_services_, "\"", "\\\"", &escaped_services);
    code = "document.getElementById('services').value = \"" + escaped_services +
           "\";";
    frame->ExecuteJavaScriptForTests(base::ASCIIToUTF16(code),
                                     base::NullCallback());
  }

  if (!test_pass_.empty()) {
    code = "document.getElementById('password').value = '" + test_pass_ + "';";
    code += "document.getElementById('nextButton').click();";
    frame->ExecuteJavaScriptForTests(base::ASCIIToUTF16(code),
                                     base::NullCallback());
  }

  // Test properties are cleared in HandleCompleteAuthentication because the
  // form submission might fail and login will not be attempted after reloading
  // if they are cleared here.
}

void GaiaScreenHandler::SetSAMLPrincipalsAPIUsed(bool is_third_party_idp,
                                                 bool is_api_used) {
  using_saml_api_ = is_api_used;
  // This correctly records the standard GAIA login and SAML flow
  // with Chrome Credentials Passing API used/not used
  RecordAPILogin(is_third_party_idp, is_api_used);
}

void GaiaScreenHandler::Show() {
  ShowScreen(GaiaView::kScreenId);
  hidden_ = false;
}

void GaiaScreenHandler::Hide() {
  hidden_ = true;
}

void GaiaScreenHandler::Bind(GaiaScreen* screen) {
  BaseScreenHandler::SetBaseScreen(screen);
}

void GaiaScreenHandler::Unbind() {
  BaseScreenHandler::SetBaseScreen(nullptr);
}

void GaiaScreenHandler::SetGaiaPath(GaiaScreenHandler::GaiaPath gaia_path) {
  gaia_path_ = gaia_path;
}

void GaiaScreenHandler::LoadGaiaAsync(const AccountId& account_id) {
  if (account_id.is_valid())
    populated_account_id_ = account_id;
  if (gaia_silent_load_ && !populated_account_id_.is_valid()) {
    dns_cleared_ = true;
    cookies_cleared_ = true;
    ShowGaiaScreenIfReady();
  } else {
    StartClearingDnsCache();
    StartClearingCookies(base::Bind(&GaiaScreenHandler::ShowGaiaScreenIfReady,
                                    weak_factory_.GetWeakPtr()));
  }
}

void GaiaScreenHandler::LoadOfflineGaia(const AccountId& account_id) {
  populated_account_id_ = account_id;
  LoadAuthExtension(true /* force */, true /* offline */);
}

void GaiaScreenHandler::ShowSigninScreenForTest(const std::string& username,
                                                const std::string& password,
                                                const std::string& services) {
  VLOG(2) << "ShowSigninScreenForTest for user " << username
          << ", frame_state=" << frame_state();

  test_user_ = username;
  test_pass_ = password;
  test_services_ = services;
  test_expects_complete_login_ = true;

  // Submit login form for test if gaia is ready. If gaia is loading, login
  // will be attempted in HandleLoginWebuiReady after gaia is ready. Otherwise,
  // reload gaia then follow the loading case.
  if (frame_state() == GaiaScreenHandler::FRAME_STATE_LOADED) {
    SubmitLoginFormForTest();
  } else if (frame_state() != GaiaScreenHandler::FRAME_STATE_LOADING &&
             !auth_extension_being_loaded_) {
    OnShowAddUser();
  }
}

void GaiaScreenHandler::ShowSecurityTokenPinDialog(
    const std::string& /*caller_extension_name*/,
    security_token_pin::CodeType code_type,
    bool enable_user_input,
    security_token_pin::ErrorLabel error_label,
    int attempts_left,
    const base::Optional<AccountId>& /*authenticating_user_account_id*/,
    SecurityTokenPinEnteredCallback pin_entered_callback,
    SecurityTokenPinDialogClosedCallback pin_dialog_closed_callback) {
  DCHECK(is_security_token_pin_enabled_);
  // There must be either no active PIN dialog, or the active dialog for which
  // the PIN has already been entered.
  DCHECK(!security_token_pin_entered_callback_);

  security_token_pin_entered_callback_ = std::move(pin_entered_callback);
  // Note that this overwrites the previous closed_callback in the case where
  // the dialog was already shown. This is intended, since the closing callback
  // should only be used to notify that the dialog got canceled, which imposes a
  // stricter quota on the PIN request caller.
  security_token_pin_dialog_closed_callback_ =
      std::move(pin_dialog_closed_callback);

  CallJS("login.GaiaSigninScreen.showPinDialog",
         MakeSecurityTokenPinDialogParameters(code_type, enable_user_input,
                                              error_label, attempts_left));
}

void GaiaScreenHandler::CloseSecurityTokenPinDialog() {
  DCHECK(is_security_token_pin_enabled_);
  // Invariant: when the pin_entered_callback is present, the closed_callback
  // must be present as well.
  DCHECK(!security_token_pin_entered_callback_ ||
         security_token_pin_dialog_closed_callback_);

  security_token_pin_entered_callback_.Reset();
  security_token_pin_dialog_closed_callback_.Reset();

  // Notify the page, unless it's already being shut down (which may happen if
  // we're called from the destructor).
  if (IsJavascriptAllowed())
    CallJS("login.GaiaSigninScreen.closePinDialog");
}

bool GaiaScreenHandler::IsOfflineLoginActive() const {
  return (screen_mode_ == GAIA_SCREEN_MODE_OFFLINE) || offline_login_is_active_;
}

void GaiaScreenHandler::SetNextSamlChallengeKeyHandlerForTesting(
    std::unique_ptr<SamlChallengeKeyHandler> handler_for_test) {
  saml_challenge_key_handler_for_test_ = std::move(handler_for_test);
}

void GaiaScreenHandler::CreateSamlChallengeKeyHandler() {
  if (saml_challenge_key_handler_for_test_) {
    saml_challenge_key_handler_ =
        std::move(saml_challenge_key_handler_for_test_);
    return;
  }
  saml_challenge_key_handler_ = std::make_unique<SamlChallengeKeyHandler>();
}

void GaiaScreenHandler::ShowGaiaScreenIfReady() {
  if (!initialized_) {
    show_on_init_ = true;
    return;
  }
  if (!dns_cleared_ || !cookies_cleared_ || !LoginDisplayHost::default_host()) {
    return;
  }

  std::string active_network_path = network_state_informer_->network_path();
  if (gaia_silent_load_ &&
      (network_state_informer_->state() != NetworkStateInformer::ONLINE ||
       gaia_silent_load_network_ != active_network_path)) {
    // Network has changed. Force Gaia reload.
    gaia_silent_load_ = false;
  }

  input_method::InputMethodManager* imm =
      input_method::InputMethodManager::Get();

  scoped_refptr<input_method::InputMethodManager::State> gaia_ime_state =
      imm->GetActiveIMEState()->Clone();
  imm->SetState(gaia_ime_state);

  // Set Least Recently Used input method for the user.
  if (populated_account_id_.is_valid()) {
    lock_screen_utils::SetUserInputMethod(populated_account_id_,
                                          gaia_ime_state.get(),
                                          true /*honor_device_policy*/);
  } else {
    std::vector<std::string> input_methods;
    if (gaia_ime_state->GetAllowedInputMethods().empty()) {
      input_methods =
          imm->GetInputMethodUtil()->GetHardwareLoginInputMethodIds();
    } else {
      input_methods = gaia_ime_state->GetAllowedInputMethods();
    }
    const std::string owner_im = lock_screen_utils::GetUserLastInputMethod(
        user_manager::UserManager::Get()->GetOwnerAccountId());
    const std::string system_im = g_browser_process->local_state()->GetString(
        language_prefs::kPreferredKeyboardLayout);

    PushFrontIMIfNotExists(owner_im, &input_methods);
    PushFrontIMIfNotExists(system_im, &input_methods);

    gaia_ime_state->EnableLoginLayouts(
        g_browser_process->GetApplicationLocale(), input_methods);

    if (!system_im.empty()) {
      gaia_ime_state->ChangeInputMethod(system_im, false /* show_message */);
    } else if (!owner_im.empty()) {
      gaia_ime_state->ChangeInputMethod(owner_im, false /* show_message */);
    }
  }

  if (!untrusted_authority_certs_cache_) {
    // Make additional untrusted authority certificates available for client
    // certificate discovery in case a SAML flow is used which requires a client
    // certificate to be present.
    // When the WebUI is destroyed, |untrusted_authority_certs_cache_| will go
    // out of scope and the certificates will not be held in memory anymore.
    untrusted_authority_certs_cache_ =
        std::make_unique<network::NSSTempCertsCacheChromeOS>(
            g_browser_process->platform_part()
                ->browser_policy_connector_chromeos()
                ->GetDeviceNetworkConfigurationUpdater()
                ->GetAllAuthorityCertificates(
                    chromeos::onc::CertificateScope::Default()));
  }

  LoadAuthExtension(!gaia_silent_load_ /* force */, false /* offline */);
  signin_screen_handler_->UpdateUIState(
      SigninScreenHandler::UI_STATE_GAIA_SIGNIN);
  core_oobe_view_->UpdateKeyboardState();

  if (gaia_silent_load_) {
    // The variable is assigned to false because silently loaded Gaia page was
    // used.
    gaia_silent_load_ = false;
  }
  UpdateState(NetworkError::ERROR_REASON_UPDATE);

  // TODO(crbug.com/1105387): Part of initial screen logic.
  if (core_oobe_view_) {
    PrefService* prefs = g_browser_process->local_state();
    if (prefs->GetBoolean(prefs::kFactoryResetRequested)) {
      core_oobe_view_->ShowDeviceResetScreen();
    }
  }
}

void GaiaScreenHandler::ShowAllowlistCheckFailedError() {
  base::DictionaryValue params;
  params.SetBoolean("enterpriseManaged",
                    g_browser_process->platform_part()
                        ->browser_policy_connector_chromeos()
                        ->IsEnterpriseManaged());

  bool family_link_allowed = false;
  CrosSettings::Get()->GetBoolean(kAccountsPrefFamilyLinkAccountsAllowed,
                                  &family_link_allowed);
  params.SetBoolean("familyLinkAllowed", family_link_allowed);

  CallJS("login.GaiaSigninScreen.showAllowlistCheckFailedError", true, params);
}

void GaiaScreenHandler::LoadAuthExtension(bool force, bool offline) {
  VLOG(1) << "LoadAuthExtension, force: " << force << ", offline: " << offline;

  if (auth_extension_being_loaded_) {
    VLOG(1) << "Skip loading the Auth extension as it's already being loaded";
    return;
  }

  auth_extension_being_loaded_ = true;
  GaiaContext context;
  context.force_reload = force;
  context.use_offline = offline;
  context.email = populated_account_id_.GetUserEmail();

  std::string gaia_id;
  if (!context.email.empty() &&
      user_manager::known_user::FindGaiaID(
          AccountId::FromUserEmail(context.email), &gaia_id)) {
    context.gaia_id = gaia_id;
  }

  if (!context.email.empty()) {
    context.gaps_cookie = user_manager::known_user::GetGAPSCookie(
        AccountId::FromUserEmail(gaia::CanonicalizeEmail(context.email)));
  }

  populated_account_id_.clear();

  LoadGaia(context);
}

void GaiaScreenHandler::SetOfflineLoginIsActive(bool is_active) {
  offline_login_is_active_ = is_active;
}

void GaiaScreenHandler::UpdateState(NetworkError::ErrorReason reason) {
  if (signin_screen_handler_ && !hidden_)
    signin_screen_handler_->UpdateState(reason);
}

bool GaiaScreenHandler::IsRestrictiveProxy() const {
  return !disable_restrictive_proxy_check_for_test_ &&
         !IsOnline(captive_portal_status_);
}

bool GaiaScreenHandler::BuildUserContextForGaiaSignIn(
    user_manager::UserType user_type,
    const AccountId& account_id,
    bool using_saml,
    const std::string& password,
    const SamlPasswordAttributes& password_attributes,
    UserContext* user_context,
    std::string* error_message) {
  *user_context = UserContext(user_type, account_id);
  if (using_saml && extension_provided_client_cert_usage_observer_ &&
      extension_provided_client_cert_usage_observer_->ClientCertsWereUsed()) {
    scoped_refptr<net::X509Certificate> saml_client_cert;
    std::vector<ChallengeResponseKey::SignatureAlgorithm> signature_algorithms;
    std::string extension_id;
    if (!extension_provided_client_cert_usage_observer_->GetOnlyUsedClientCert(
            &saml_client_cert, &signature_algorithms, &extension_id)) {
      *error_message = l10n_util::GetStringUTF8(
          IDS_CHALLENGE_RESPONSE_AUTH_MULTIPLE_CLIENT_CERTS_ERROR);
      return false;
    }
    ChallengeResponseKey challenge_response_key;
    if (!ExtractChallengeResponseKeyFromCert(
            *saml_client_cert, signature_algorithms, &challenge_response_key)) {
      *error_message = l10n_util::GetStringUTF8(
          IDS_CHALLENGE_RESPONSE_AUTH_INVALID_CLIENT_CERT_ERROR);
      return false;
    }
    challenge_response_key.set_extension_id(extension_id);
    user_context->GetMutableChallengeResponseKeys()->push_back(
        challenge_response_key);
  } else {
    Key key(password);
    key.SetLabel(kCryptohomeGaiaKeyLabel);
    user_context->SetKey(key);
    user_context->SetPasswordKey(Key(password));
  }
  user_context->SetAuthFlow(using_saml
                                ? UserContext::AUTH_FLOW_GAIA_WITH_SAML
                                : UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);
  if (using_saml)
    user_context->SetIsUsingSamlPrincipalsApi(using_saml_api_);
  if (using_saml && ExtractSamlPasswordAttributesEnabled()) {
    user_context->SetSamlPasswordAttributes(password_attributes);
  }
  return true;
}

}  // namespace chromeos
