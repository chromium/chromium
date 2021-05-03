// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/constants/devicetype.h"
#include "ash/public/cpp/login_screen.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/guid.h"
#include "base/i18n/message_formatter.h"
#include "base/i18n/number_formatting.h"
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
#include "chrome/browser/ash/authpolicy/authpolicy_helper.h"
#include "chrome/browser/ash/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/ash/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/ash/certificate_provider/pin_dialog_manager.h"
#include "chrome/browser/ash/login/reauth_stats.h"
#include "chrome/browser/ash/login/saml/public_saml_url_fetcher.h"
#include "chrome/browser/ash/login/saml/saml_metric_utils.h"
#include "chrome/browser/ash/login/screens/network_error.h"
#include "chrome/browser/ash/login/screens/signin_fatal_error_screen.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/signin_partition_manager.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/ui/login_display_host_webui.h"
#include "chrome/browser/ash/login/ui/user_adding_screen.h"
#include "chrome/browser/ash/login/users/chrome_user_manager.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_network_configuration_updater.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "chrome/browser/ui/webui/chromeos/login/cookie_waiter.h"
#include "chrome/browser/ui/webui/chromeos/login/enrollment_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/reset_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_fatal_error_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/user_creation_screen_handler.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chromeos/components/security_token_pin/constants.h"
#include "chromeos/components/security_token_pin/error_generator.h"
#include "chromeos/dbus/util/version_loader.h"
#include "chromeos/login/auth/challenge_response/cert_utils.h"
#include "chromeos/login/auth/cryptohome_key_constants.h"
#include "chromeos/login/auth/saml_password_attributes.h"
#include "chromeos/login/auth/sync_trusted_vault_keys.h"
#include "chromeos/login/auth/user_context.h"
#include "chromeos/network/onc/certificate_scope.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/login/localized_values_builder.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_driver_switches.h"
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
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"

using content::BrowserThread;
namespace em = enterprise_management;

namespace chromeos {

namespace {

const char kAuthIframeParentName[] = "signin-frame";

const char kEndpointGen[] = "1.0";

bool IsSyncTrustedVaultKeysEnabled() {
  return base::FeatureList::IsEnabled(
      ::switches::kSyncSupportTrustedVaultPassphraseRecovery);
}

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

GaiaScreenHandler::GaiaScreenMode GetGaiaScreenMode(const std::string& email) {
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

std::string GetEnterpriseDomainManager() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return connector->GetEnterpriseDomainManager();
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

std::string GetSSOProfile() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return connector->GetSSOProfile();
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

bool IsOnline(NetworkPortalDetector::CaptivePortalStatus status) {
  return status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE;
}

void GetVersionAndConsent(std::string* out_version, bool* out_consent) {
  *out_version = version_loader::GetVersion(version_loader::VERSION_SHORT);
  *out_consent = GoogleUpdateSettings::GetCollectStatsConsent();
}

user_manager::UserType CalculateUserType(const AccountId& account_id) {
  if (user_manager::UserManager::Get()->IsDeprecatedSupervisedAccountId(
          account_id)) {
    return user_manager::USER_TYPE_SUPERVISED_DEPRECATED;
  }

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

PinDialogManager* GetLoginScreenPinDialogManager() {
  DCHECK(ProfileHelper::IsSigninProfileInitialized());
  CertificateProviderService* certificate_provider_service =
      CertificateProviderServiceFactory::GetForBrowserContext(
          ProfileHelper::GetSigninProfile());
  return certificate_provider_service->pin_dialog_manager();
}

base::Value MakeSecurityTokenPinDialogParameters(
    bool enable_user_input,
    security_token_pin::ErrorLabel error_label,
    int attempts_left) {
  base::Value params(base::Value::Type::DICTIONARY);

  params.SetBoolKey("enableUserInput", enable_user_input);
  params.SetBoolKey("hasError",
                    error_label != security_token_pin::ErrorLabel::kNone);
  params.SetStringKey(
      "formattedError",
      GenerateErrorMessage(error_label, attempts_left, enable_user_input));
  if (attempts_left == -1) {
    params.SetStringKey("formattedAttemptsLeft", std::u16string());
  } else {
    params.SetStringKey(
        "formattedAttemptsLeft",
        GenerateErrorMessage(security_token_pin::ErrorLabel::kNone,
                             attempts_left, true));
  }
  return params;
}

}  // namespace

constexpr StaticOobeScreenId GaiaView::kScreenId;

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
  if (is_security_token_pin_enabled_)
    GetLoginScreenPinDialogManager()->RemovePinDialogHost(this);
}

void GaiaScreenHandler::DisableRestrictiveProxyCheckForTest() {
  disable_restrictive_proxy_check_for_test_ = true;
}

void GaiaScreenHandler::LoadGaia(const login::GaiaContext& context) {
  auto partition_call = login::GetStartSigninSession(
      web_ui(), base::BindOnce(&GaiaScreenHandler::LoadGaiaWithPartition,
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
    const login::GaiaContext& context,
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

  login::SetCookieForPartition(context, signin_partition_manager,
                               std::move(callback));
}

void GaiaScreenHandler::OnSetCookieForLoadGaiaWithPartition(
    const login::GaiaContext& context,
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
    const login::GaiaContext& context,
    const std::string& partition_name,
    const std::string* platform_version,
    const bool* collect_stats_consent) {
  base::DictionaryValue params;

  params.SetBoolean("forceReload", context.force_reload);
  params.SetString("gaiaId", context.gaia_id);
  params.SetBoolean("readOnlyEmail", true);
  params.SetString("email", context.email);

  UpdateAuthParams(&params, IsRestrictiveProxy());

  screen_mode_ = GetGaiaScreenMode(context.email);
  params.SetInteger("screenMode", screen_mode_);

  const std::string app_locale = g_browser_process->GetApplicationLocale();
  if (!app_locale.empty())
    params.SetString("hl", app_locale);

  std::string realm(GetRealm());
  if (!realm.empty()) {
    params.SetString("realm", realm);
  }

  const std::string enterprise_display_domain(GetEnterpriseDisplayDomain());
  const std::string enterprise_enrollment_domain(
      GetEnterpriseEnrollmentDomain());
  const std::string enterprise_domain_manager(GetEnterpriseDomainManager());
  const std::string sso_profile(GetSSOProfile());

  if (!enterprise_display_domain.empty())
    params.SetString("enterpriseDisplayDomain", enterprise_display_domain);
  if (!enterprise_enrollment_domain.empty()) {
    params.SetString("enterpriseEnrollmentDomain",
                     enterprise_enrollment_domain);
  }
  if (!sso_profile.empty()) {
    params.SetString("ssoProfile", sso_profile);
  }
  if (!enterprise_domain_manager.empty()) {
    params.SetString("enterpriseDomainManager", enterprise_domain_manager);
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
  // Extended stable channel is not supported on Chrome OS Ash.
  params.SetString("releaseChannel",
                   chrome::GetChannelName(chrome::WithExtendedStable(false)));
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
    case GaiaPath::kReauth:
      params.SetString(
          "gaiaPath",
          GaiaUrls::GetInstance()->embedded_reauth_chromeos_url().path().substr(
              1));
      break;
  }

  // We only send `chromeos_board` Gaia URL parameter if user has opted into
  // sending device statistics.
  if (*collect_stats_consent)
    params.SetString("lsbReleaseBoard", base::SysInfo::GetLsbReleaseBoard());

  params.SetString("webviewPartitionName", partition_name);
  signin_partition_name_ = partition_name;

  params.SetBoolean("extractSamlPasswordAttributes",
                    login::ExtractSamlPasswordAttributesEnabled());
  params.SetBoolean("enableSyncTrustedVaultKeys",
                    IsSyncTrustedVaultKeysEnabled());

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

  bool is_reauth = !context.email.empty();
  if (is_reauth && features::IsGaiaReauthEndpointEnabled()) {
    const AccountId account_id =
        GetAccountId(context.email, context.gaia_id, AccountType::GOOGLE);
    auto* user = user_manager::UserManager::Get()->FindUser(account_id);
    DCHECK(user);
    bool is_child_account = user && user->IsChild();
    // Enable the new endpoint for supervised account for now. We might expand
    // it to other account type in the future.
    if (is_child_account) {
      params.SetBoolean("isSupervisedUser", is_child_account);
      params.SetBoolean(
          "isDeviceOwner",
          account_id == user_manager::UserManager::Get()->GetOwnerAccountId());
    }
  }

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
  LoadAuthExtension(force_reload);
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

  builder->AddF("loginWelcomeMessage", IDS_LOGIN_WELCOME_MESSAGE,
                ui::GetChromeOSDeviceTypeResourceId());
  builder->Add("enterpriseInfoMessage", IDS_LOGIN_DEVICE_MANAGED_BY_NOTICE);
  builder->Add("samlInterstitialMessage", IDS_LOGIN_SAML_INTERSTITIAL_MESSAGE);
  builder->Add("samlInterstitialChangeAccountLink",
               IDS_LOGIN_SAML_INTERSTITIAL_CHANGE_ACCOUNT_LINK_TEXT);
  builder->Add("samlInterstitialNextBtn",
               IDS_LOGIN_SAML_INTERSTITIAL_NEXT_BUTTON_TEXT);

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
  AddCallback("authExtensionLoaded",
              &GaiaScreenHandler::HandleAuthExtensionLoaded);
  AddRawCallback("showAddUser", &GaiaScreenHandler::HandleShowAddUser);
  AddCallback("getIsSamlUserPasswordless",
              &GaiaScreenHandler::HandleGetIsSamlUserPasswordless);
  AddCallback("setIsFirstSigninStep",
              &GaiaScreenHandler::HandleIsFirstSigninStep);
  AddCallback("samlStateChanged", &GaiaScreenHandler::HandleSamlStateChanged);
  AddCallback("securityTokenPinEntered",
              &GaiaScreenHandler::HandleSecurityTokenPinEntered);
  AddCallback("onFatalError", &GaiaScreenHandler::HandleOnFatalError);
  AddCallback("removeUserByEmail", &GaiaScreenHandler::HandleUserRemoved);

  BaseScreenHandler::RegisterMessages();
}

void GaiaScreenHandler::OnPortalDetectionCompleted(
    const NetworkState* network,
    const NetworkPortalDetector::CaptivePortalStatus status) {
  VLOG(1) << "OnPortalDetectionCompleted "
          << NetworkPortalDetector::CaptivePortalStatusString(status);

  const NetworkPortalDetector::CaptivePortalStatus previous_status =
      captive_portal_status_;
  captive_portal_status_ = status;
  if (IsOnline(captive_portal_status_) == IsOnline(previous_status) ||
      disable_restrictive_proxy_check_for_test_ ||
      GetCurrentScreen() != kScreenId)
    return;

  LoadAuthExtension(true /* force */);
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

void GaiaScreenHandler::HandleCompleteAuthentication(
    const std::string& gaia_id,
    const std::string& email,
    const std::string& password,
    bool using_saml,
    const ::login::StringList& services,
    const base::DictionaryValue* password_attributes,
    const base::DictionaryValue* sync_trusted_vault_keys) {
  if (!LoginDisplayHost::default_host())
    return;

  DCHECK(!email.empty());
  DCHECK(!gaia_id.empty());

  // Execute delayed allowlist check that is based on user type.
  const user_manager::UserType user_type =
      login::GetUsertypeFromServicesString(services);
  if (ShouldCheckUserTypeBeforeAllowing() &&
      !LoginDisplayHost::default_host()->IsUserAllowlisted(
          GetAccountId(email, gaia_id, AccountType::GOOGLE), user_type)) {
    ShowAllowlistCheckFailedError();
    return;
  }

  // Record amount of time from the moment screen was shown till
  // completeAuthentication signal come. Only for no SAML flow and only during
  // first run in OOBE.
  if (elapsed_timer_ && !using_saml &&
      session_manager::SessionManager::Get()->session_state() ==
          session_manager::SessionState::OOBE) {
    base::UmaHistogramMediumTimes("OOBE.GaiaLoginTime",
                                  elapsed_timer_->Elapsed());
    elapsed_timer_.reset();
  }

  const std::string sanitized_email = gaia::SanitizeEmail(email);
  LoginDisplayHost::default_host()->SetDisplayEmail(sanitized_email);

  login::SigninPartitionManager* signin_partition_manager =
      login::SigninPartitionManager::Factory::GetForBrowserContext(
          Profile::FromWebUI(web_ui()));
  online_login_helper_ = std::make_unique<OnlineLoginHelper>(
      signin_partition_name_, signin_partition_manager,
      base::BindOnce(&GaiaScreenHandler::OnCookieWaitTimeout,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&LoginDisplayHost::CompleteLogin,
                     base::Unretained(LoginDisplayHost::default_host())));

  pending_user_context_ = std::make_unique<UserContext>();
  std::string error_message;
  if (!login::BuildUserContextForGaiaSignIn(
          login::GetUsertypeFromServicesString(services),
          GetAccountId(email, gaia_id, AccountType::GOOGLE), using_saml,
          using_saml_api_, password,
          SamlPasswordAttributes::FromJs(*password_attributes),
          IsSyncTrustedVaultKeysEnabled()
              ? base::make_optional(
                    SyncTrustedVaultKeys::FromJs(*sync_trusted_vault_keys))
              : base::nullopt,
          *extension_provided_client_cert_usage_observer_,
          pending_user_context_.get(), &error_message)) {
    core_oobe_view_->ShowSignInError(0, error_message, std::string(),
                                     HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT);
    pending_user_context_.reset();
    return;
  }

  online_login_helper_->SetUserContext(std::move(pending_user_context_));
  online_login_helper_->RequestCookiesAndCompleteAuthentication();

  if (test_expects_complete_login_) {
    VLOG(2) << "Complete test login for " << sanitized_email
            << ", requested=" << test_user_;

    test_expects_complete_login_ = false;
    test_user_.clear();
    test_pass_.clear();
  }
}

void GaiaScreenHandler::OnCookieWaitTimeout() {
  LoadAuthExtension(true /* force */);
  core_oobe_view_->ShowSignInError(
      0, l10n_util::GetStringUTF8(IDS_LOGIN_FATAL_ERROR_NO_AUTH_TOKEN),
      std::string(), HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT);
}

void GaiaScreenHandler::HandleCompleteLogin(const std::string& gaia_id,
                                            const std::string& typed_email,
                                            const std::string& password,
                                            bool using_saml) {
  VLOG(1) << "HandleCompleteLogin";
  DoCompleteLogin(gaia_id, typed_email, password, using_saml);
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
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT0(
      "ui", "ShowAddUser",
      TRACE_ID_WITH_SCOPE(LoginDisplayHostWebUI::kShowLoginWebUIid,
                          TRACE_ID_GLOBAL(1)));

  std::string email;
  // `args` can be null if it's OOBE.
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

void GaiaScreenHandler::HandleIsFirstSigninStep(bool is_first) {
  ash::LoginScreen::Get()->SetIsFirstSigninStep(is_first);
}

void GaiaScreenHandler::HandleSamlStateChanged(bool is_saml) {
  if (is_saml == is_security_token_pin_enabled_) {
    // We're already in the needed `is_security_token_pin_enabled_` state.
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
    // Keep `security_token_pin_dialog_closed_callback_`, in order to be able to
    // notify about the dialog closing afterwards.
  }
}

void GaiaScreenHandler::HandleOnFatalError(
    int error_code,
    const base::DictionaryValue* params) {
  LoginDisplayHost::default_host()
      ->GetWizardController()
      ->ShowSignInFatalErrorScreen(SignInFatalErrorScreen::Error(error_code),
                                   params);
}

void GaiaScreenHandler::HandleUserRemoved(const std::string& email) {
  const AccountId account_id = user_manager::known_user::GetAccountId(
      email, /*id=*/std::string(), AccountType::UNKNOWN);
  if (account_id == user_manager::UserManager::Get()->GetOwnerAccountId()) {
    // Shows powerwash UI if the user is device owner.
    DCHECK(LoginDisplayHost::default_host());
    LoginDisplayHost::default_host()->StartWizard(ResetView::kScreenId);
  } else {
    // Removes the account on the device.
    user_manager::UserManager::Get()->RemoveUser(account_id,
                                                 nullptr /*delegate*/);
  }
}

void GaiaScreenHandler::OnShowAddUser() {
  LoginDisplayHost::default_host()->ShowGaiaDialog(populated_account_id_);
}

void GaiaScreenHandler::DoCompleteLogin(const std::string& gaia_id,
                                        const std::string& typed_email,
                                        const std::string& password,
                                        bool using_saml) {
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
  if (!login::BuildUserContextForGaiaSignIn(
          user ? user->GetType() : CalculateUserType(account_id),
          GetAccountId(typed_email, gaia_id, AccountType::GOOGLE), using_saml,
          using_saml_api_, password, SamlPasswordAttributes(),
          /*sync_trusted_vault_keys=*/base::nullopt,
          *extension_provided_client_cert_usage_observer_, &user_context,
          &error_message)) {
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
    base::OnceClosure on_clear_callback) {
  cookies_cleared_ = false;
  ProfileHelper* profile_helper = ProfileHelper::Get();
  LOG_ASSERT(Profile::FromWebUI(web_ui()) ==
             profile_helper->GetSigninProfile());
  profile_helper->ClearSigninProfile(
      base::BindOnce(&GaiaScreenHandler::OnCookiesCleared,
                     weak_factory_.GetWeakPtr(), std::move(on_clear_callback)));
}

void GaiaScreenHandler::OnCookiesCleared(base::OnceClosure on_clear_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  cookies_cleared_ = true;
  std::move(on_clear_callback).Run();
}

void GaiaScreenHandler::SubmitLoginFormForTest() {
  VLOG(2) << "Submit login form for test, user=" << test_user_;

  content::RenderFrameHost* frame =
      signin::GetAuthFrame(web_ui()->GetWebContents(), kAuthIframeParentName);

  // clang-format off
  std::string code =
      "document.getElementById('identifier').value = '" + test_user_ + "';";
  // clang-format on

  frame->ExecuteJavaScriptForTests(base::ASCIIToUTF16(code),
                                   base::NullCallback());
  CallJS("login.GaiaSigninScreen.clickPrimaryButtonForTesting");

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
    frame->ExecuteJavaScriptForTests(base::ASCIIToUTF16(code),
                                     base::NullCallback());
    CallJS("login.GaiaSigninScreen.clickPrimaryButtonForTesting");
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
  elapsed_timer_ = std::make_unique<base::ElapsedTimer>();
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
  populated_account_id_ = account_id;
  if (gaia_silent_load_ && !populated_account_id_.is_valid()) {
    dns_cleared_ = true;
    cookies_cleared_ = true;
    ShowGaiaScreenIfReady();
  } else {
    StartClearingDnsCache();
    StartClearingCookies(base::BindOnce(
        &GaiaScreenHandler::ShowGaiaScreenIfReady, weak_factory_.GetWeakPtr()));
  }
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
         MakeSecurityTokenPinDialogParameters(enable_user_input, error_label,
                                              attempts_left));
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

  if (!untrusted_authority_certs_cache_) {
    // Make additional untrusted authority certificates available for client
    // certificate discovery in case a SAML flow is used which requires a client
    // certificate to be present.
    // When the WebUI is destroyed, `untrusted_authority_certs_cache_` will go
    // out of scope and the certificates will not be held in memory anymore.
    untrusted_authority_certs_cache_ =
        std::make_unique<network::NSSTempCertsCacheChromeOS>(
            g_browser_process->platform_part()
                ->browser_policy_connector_chromeos()
                ->GetDeviceNetworkConfigurationUpdater()
                ->GetAllAuthorityCertificates(
                    chromeos::onc::CertificateScope::Default()));
  }

  LoadAuthExtension(!gaia_silent_load_ /* force */);
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

void GaiaScreenHandler::LoadAuthExtension(bool force) {
  VLOG(1) << "LoadAuthExtension, force: " << force;
  if (auth_extension_being_loaded_) {
    VLOG(1) << "Skip loading the Auth extension as it's already being loaded";
    return;
  }

  auth_extension_being_loaded_ = true;
  login::GaiaContext context;
  context.force_reload = force;
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

void GaiaScreenHandler::UpdateState(NetworkError::ErrorReason reason) {
  if (signin_screen_handler_ && !hidden_)
    signin_screen_handler_->UpdateState(reason);
}

bool GaiaScreenHandler::IsRestrictiveProxy() const {
  return !disable_restrictive_proxy_check_for_test_ &&
         !IsOnline(captive_portal_status_);
}

}  // namespace chromeos
