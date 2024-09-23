// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/inline_login_handler_impl.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/new_tab_page/chrome_colors/selected_colors_info.h"
#include "chrome/browser/password_manager/password_reuse_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/about_signin_internals_factory.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/profiles/profile_colors_util.h"
#include "chrome/browser/ui/profiles/profile_customization_bubble_sync_controller.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
#include "chrome/browser/ui/webui/signin/signin_url_utils.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/browser/ui/webui/signin/signin_utils_desktop.h"
#include "chrome/browser/ui/webui/signin/turn_sync_on_helper.h"
#include "chrome/browser/ui/webui/signin/turn_sync_on_helper_delegate_impl.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_reuse_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/about_signin_internals.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/string_split.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#endif  // BUILDFLAG(IS_WIN)

namespace {

// Subset of signin_metrics::Reason that is supported by the
// InlineLoginHandlerImpl.
enum class HandlerSigninReason {
  kForcedSigninPrimaryAccount,
  kReauthentication,
  kFetchLstOnly
};

// Decodes the signin reason from the URL parameter.
HandlerSigninReason GetHandlerSigninReason(const GURL& url) {
  signin_metrics::Reason reason =
      signin::GetSigninReasonForEmbeddedPromoURL(url);
  switch (reason) {
    case signin_metrics::Reason::kForcedSigninPrimaryAccount:
      return HandlerSigninReason::kForcedSigninPrimaryAccount;
    case signin_metrics::Reason::kReauthentication:
      return HandlerSigninReason::kReauthentication;
    case signin_metrics::Reason::kFetchLstOnly:
      return HandlerSigninReason::kFetchLstOnly;
    default:
      NOTREACHED_IN_MIGRATION()
          << "Unexpected signin reason: " << static_cast<int>(reason);
      return HandlerSigninReason::kForcedSigninPrimaryAccount;
  }
}

// Specific implementation of TurnSyncOnHelper::Delegate for forced
// signin flows. Some confirmation prompts are skipped.
class ForcedSigninTurnSyncOnHelperDelegate
    : public TurnSyncOnHelperDelegateImpl {
 public:
  explicit ForcedSigninTurnSyncOnHelperDelegate(Browser* browser)
      : TurnSyncOnHelperDelegateImpl(browser, /*is_sync_promo=*/false) {}

 protected:
  void ShouldEnterpriseConfirmationPromptForNewProfile(
      Profile* profile,
      base::OnceCallback<void(bool)> callback) override {
    std::move(callback).Run(/*prompt_for_new_profile=*/false);
  }

 private:
  void ShowMergeSyncDataConfirmation(
      const std::string& previous_email,
      const std::string& new_email,
      signin::SigninChoiceCallback callback) override {
    NOTREACHED_IN_MIGRATION();
  }
};

#if BUILDFLAG(IS_WIN)

// Returns a list of valid signin domains that were passed in
// |email_domains_parameter| as an argument to the gcpw signin dialog.

std::vector<std::string> GetEmailDomainsFromParameter(
    const std::string& email_domains_parameter) {
  return base::SplitString(base::ToLowerASCII(email_domains_parameter),
                           credential_provider::kEmailDomainsSeparator,
                           base::WhitespaceHandling::TRIM_WHITESPACE,
                           base::SplitResult::SPLIT_WANT_NONEMPTY);
}

// Validates that the |signin_gaia_id| that the user signed in with matches
// the |gaia_id_parameter| passed to the gcpw signin dialog. Also ensures
// that the |signin_email| is in a domain listed in |email_domains_parameter|.
// Returns kUiecSuccess on success.
// Returns the appropriate error code on failure.
credential_provider::UiExitCodes ValidateSigninEmail(
    const std::string& gaia_id_parameter,
    const std::string& email_domains_parameter,
    const std::string& signin_email,
    const std::string& signin_gaia_id) {
  if (!gaia_id_parameter.empty() &&
      !base::EqualsCaseInsensitiveASCII(gaia_id_parameter, signin_gaia_id)) {
    return credential_provider::kUiecEMailMissmatch;
  }

  if (email_domains_parameter.empty())
    return credential_provider::kUiecSuccess;

  std::vector<std::string> all_email_domains =
      GetEmailDomainsFromParameter(email_domains_parameter);
  std::string email_domain = gaia::ExtractDomainName(signin_email);

  return base::Contains(all_email_domains, email_domain)
             ? credential_provider::kUiecSuccess
             : credential_provider::kUiecInvalidEmailDomain;
}

#endif

void SetProfileLocked(const base::FilePath profile_path, bool locked) {
  if (profile_path.empty())
    return;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)
    return;

  ProfileAttributesEntry* entry =
      profile_manager->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_path);
  if (!entry)
    return;

  if (signin_util::IsForceSigninEnabled())
    entry->LockForceSigninProfile(locked);
}

void UnlockProfileAndHideLoginUI(const base::FilePath profile_path,
                                 InlineLoginHandlerImpl* handler) {
  SetProfileLocked(profile_path, false);
  handler->CloseDialogFromJavascript();
  ProfilePicker::Hide();
}

void LockProfileAndShowUserManager(const base::FilePath& profile_path) {
  SetProfileLocked(profile_path, true);
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileLocked));
}

// Callback for DiceTurnOnSyncHelper.
void OnSigninComplete(Profile* profile,
                      const std::string& username,
                      const std::string& password,
                      bool is_force_sign_in_with_usermanager) {
  DCHECK(signin_util::IsForceSigninEnabled());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  bool can_be_managed = enterprise_util::ProfileCanBeManaged(profile);
  if (can_be_managed && !password.empty()) {
    password_manager::PasswordReuseManager* reuse_manager =
        PasswordReuseManagerFactory::GetForProfile(profile);
    if (reuse_manager) {
      reuse_manager->SaveGaiaPasswordHash(
          username, base::UTF8ToUTF16(password),
          /*is_sync_password_for_metrics=*/true,
          password_manager::metrics_util::GaiaPasswordHashChange::
              SAVED_ON_CHROME_SIGNIN);
    }
  }

  if (can_be_managed && is_force_sign_in_with_usermanager) {
    CoreAccountInfo primary_account =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
    AccountInfo primary_account_info =
        identity_manager->FindExtendedAccountInfo(primary_account);
    std::u16string profile_name;
    if (!primary_account_info.IsEmpty()) {
      profile_name =
          profiles::GetDefaultNameForNewSignedInProfile(primary_account_info);
    } else {
      profile_name =
          profiles::GetDefaultNameForNewSignedInProfileWithIncompleteInfo(
              primary_account);
    }
    ProfileAttributesEntry* entry =
        g_browser_process->profile_manager()
            ->GetProfileAttributesStorage()
            .GetProfileAttributesWithPath(profile->GetPath());
    if (entry) {
      entry->SetLocalProfileName(profile_name, /*is_default_name=*/false);
      // TODO(crbug.com/40172714): move the following code into a new
      // `Profile::SetIsHidden()` method.
      entry->SetIsOmitted(false);
      if (!profile->GetPrefs()->GetBoolean(prefs::kForceEphemeralProfiles)) {
        // Unmark this profile ephemeral so that it isn't deleted upon next
        // startup. Profiles should never be made non-ephemeral if ephemeral
        // mode is forced by policy.
        entry->SetIsEphemeral(false);
      }
    } else {
      DVLOG(1) << "ProfileAttributesEntry not found for profile:"
               << profile->GetPath();
    }

    Browser* browser = chrome::FindBrowserWithProfile(profile);

    // Don't show the customization bubble if a valid policy theme is set.
    if (browser &&
        !ThemeServiceFactory::GetForProfile(profile)->UsingPolicyTheme()) {
      ApplyProfileColorAndShowCustomizationBubbleWhenNoValueSynced(
          browser, GenerateNewProfileColor().color);
    }
  }

  if (!can_be_managed) {
    BrowserList::CloseAllBrowsersWithProfile(
        profile, base::BindRepeating(&LockProfileAndShowUserManager),
        // Cannot be called because skip_beforeunload is true.
        BrowserList::CloseCallback(),
        /*skip_beforeunload=*/true);
  }
}

}  // namespace

InlineSigninHelper::InlineSigninHelper(
    base::WeakPtr<InlineLoginHandlerImpl> handler,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    Profile* profile,
    const GURL& current_url,
    const std::string& email,
    const std::string& gaia_id,
    const std::string& password,
    const std::string& auth_code,
    const std::string& signin_scoped_device_id,
    bool confirm_untrusted_signin,
    bool is_force_sign_in_with_usermanager)
    : gaia_auth_fetcher_(this, gaia::GaiaSource::kChrome, url_loader_factory),
      handler_(handler),
      profile_(profile),
      current_url_(current_url),
      email_(email),
      gaia_id_(gaia_id),
      password_(password),
      auth_code_(auth_code),
      confirm_untrusted_signin_(confirm_untrusted_signin),
      is_force_sign_in_with_usermanager_(is_force_sign_in_with_usermanager) {
  DCHECK(profile_);
  DCHECK(!email_.empty());
  DCHECK(!auth_code_.empty());
  DCHECK(handler);

  gaia_auth_fetcher_.StartAuthCodeForOAuth2TokenExchangeWithDeviceId(
      auth_code_, signin_scoped_device_id);
}

InlineSigninHelper::~InlineSigninHelper() {}

void InlineSigninHelper::OnClientOAuthSuccess(const ClientOAuthResult& result) {
  if (is_force_sign_in_with_usermanager_) {
    // If user sign in in UserManager with force sign in enabled, the browser
    // window won't be opened until now.
    UnlockProfileAndHideLoginUI(profile_->GetPath(), handler_.get());
    // TODO(crbug.com/40764426): In case of reauth, wait until cookies
    // are set before opening a browser window.
    profiles::OpenBrowserWindowForProfile(
        base::IgnoreArgs<Browser*>(base::BindOnce(
            &InlineSigninHelper::OnClientOAuthSuccessAndBrowserOpened,
            base::Unretained(this), result)),
        true, false, true, profile_);
  } else {
    OnClientOAuthSuccessAndBrowserOpened(result);
  }
}

void InlineSigninHelper::OnClientOAuthSuccessAndBrowserOpened(
    const ClientOAuthResult& result) {
  HandlerSigninReason reason = GetHandlerSigninReason(current_url_);
  if (reason == HandlerSigninReason::kFetchLstOnly) {
    // Constants are only available on Windows for the Google Credential
    // Provider for Windows.
#if BUILDFLAG(IS_WIN)
    std::string json_retval;
    base::Value::Dict args;
    args.Set(credential_provider::kKeyEmail, base::Value(email_));
    args.Set(credential_provider::kKeyPassword, base::Value(password_));
    args.Set(credential_provider::kKeyId, base::Value(gaia_id_));
    args.Set(credential_provider::kKeyRefreshToken,
             base::Value(result.refresh_token));
    args.Set(credential_provider::kKeyAccessToken,
             base::Value(result.access_token));

    handler_->SendLSTFetchResultsMessage(base::Value(std::move(args)));
#else
    NOTREACHED_IN_MIGRATION()
        << "Google Credential Provider is only available on Windows";
#endif  // BUILDFLAG(IS_WIN)
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                  this);
    return;
  }

  AboutSigninInternals* about_signin_internals =
      AboutSigninInternalsFactory::GetForProfile(profile_);
  about_signin_internals->OnRefreshTokenReceived("Successful");

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);

  std::string sync_email =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSync)
          .email;

  if (!password_.empty()) {
    password_manager::PasswordReuseManager* reuse_manager =
        PasswordReuseManagerFactory::GetForProfile(profile_);
    if (reuse_manager) {
      reuse_manager->SaveGaiaPasswordHash(
          sync_email, base::UTF8ToUTF16(password_),
          /*is_sync_password_for_metrics=*/!sync_email.empty(),
          password_manager::metrics_util::GaiaPasswordHashChange::
              SAVED_ON_CHROME_SIGNIN);
    }
  }

  if (reason == HandlerSigninReason::kReauthentication) {
    DCHECK(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin) &&
           enterprise_util::UserAcceptedAccountManagement(profile_));
    // TODO(b/278545484): support LST binding for refresh tokens created by
    // InlineSigninHelper.
    identity_manager->GetAccountsMutator()->AddOrUpdateAccount(
        gaia_id_, email_, result.refresh_token,
        result.is_under_advanced_protection,
        signin_metrics::AccessPoint::ACCESS_POINT_FORCED_SIGNIN,
        signin_metrics::SourceForRefreshTokenOperation::
            kInlineLoginHandler_Signin);
  } else {
    if (confirm_untrusted_signin_) {
      // Display a confirmation dialog to the user.
      base::RecordAction(
          base::UserMetricsAction("Signin_Show_UntrustedSigninPrompt"));
      Browser* browser = chrome::FindLastActiveWithProfile(profile_);
      browser->window()->ShowOneClickSigninConfirmation(
          base::UTF8ToUTF16(email_),
          base::BindOnce(&InlineSigninHelper::UntrustedSigninConfirmed,
                         base::Unretained(this), result.refresh_token));
      return;
    }
    CreateSyncStarter(result.refresh_token);
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                this);
}

void InlineSigninHelper::UntrustedSigninConfirmed(
    const std::string& refresh_token,
    bool confirmed) {
  DCHECK(signin_util::IsForceSigninEnabled());
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                this);
  if (confirmed) {
    CreateSyncStarter(refresh_token);
    return;
  }

  base::RecordAction(base::UserMetricsAction("Signin_Undo_Signin"));
  BrowserList::CloseAllBrowsersWithProfile(
      profile_, base::BindRepeating(&LockProfileAndShowUserManager),
      // Cannot be called because  skip_beforeunload is true.
      BrowserList::CloseCallback(),
      /*skip_beforeunload=*/true);
}

void InlineSigninHelper::CreateSyncStarter(const std::string& refresh_token) {
  DCHECK(signin_util::IsForceSigninEnabled());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    // Already signed in, nothing to do.
    return;
  }

  Browser* browser = chrome::FindLastActiveWithProfile(profile_);
  // TODO(b/278545484): support LST binding for refresh tokens created by
  // InlineSigninHelper.
  CoreAccountId account_id =
      identity_manager->GetAccountsMutator()->AddOrUpdateAccount(
          gaia_id_, email_, refresh_token,
          /*is_under_advanced_protection=*/false,
          signin_metrics::AccessPoint::ACCESS_POINT_FORCED_SIGNIN,
          signin_metrics::SourceForRefreshTokenOperation::
              kInlineLoginHandler_Signin);

  std::unique_ptr<TurnSyncOnHelper::Delegate> delegate =
      std::make_unique<ForcedSigninTurnSyncOnHelperDelegate>(browser);

  new TurnSyncOnHelper(
      profile_, signin::GetAccessPointForEmbeddedPromoURL(current_url_),
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO, account_id,
      TurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT, std::move(delegate),
      base::BindOnce(&OnSigninComplete, profile_, email_, password_,
                     is_force_sign_in_with_usermanager_));
}

void InlineSigninHelper::OnClientOAuthFailure(
    const GoogleServiceAuthError& error) {
  if (handler_) {
    handler_->HandleLoginError(
        SigninUIError::FromGoogleServiceAuthError(email_, error));
  }

  HandlerSigninReason reason = GetHandlerSigninReason(current_url_);
  if (reason != HandlerSigninReason::kFetchLstOnly) {
    AboutSigninInternals* about_signin_internals =
        AboutSigninInternalsFactory::GetForProfile(profile_);
    about_signin_internals->OnRefreshTokenReceived("Failure");
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                this);
}

InlineLoginHandlerImpl::InlineLoginHandlerImpl()
    : confirm_untrusted_signin_(false) {}

InlineLoginHandlerImpl::~InlineLoginHandlerImpl() {}

// static
void InlineLoginHandlerImpl::SetExtraInitParams(base::Value::Dict& params) {
  params.Set("service", "chromiumsync");

  // If this was called from the user manager to reauthenticate the profile,
  // make sure the webui is aware.
  content::WebContents* contents = web_ui()->GetWebContents();
  const GURL& current_url = contents->GetLastCommittedURL();
  if (HasFromProfilePickerURLParameter(current_url))
    params.Set("dontResizeNonEmbeddedPages", true);

  HandlerSigninReason reason = GetHandlerSigninReason(current_url);

  const GURL& url = GaiaUrls::GetInstance()->embedded_signin_url();
  params.Set("clientId", GaiaUrls::GetInstance()->oauth2_chrome_client_id());
  params.Set("gaiaPath", url.path().substr(1));

#if BUILDFLAG(IS_WIN)
  if (reason == HandlerSigninReason::kFetchLstOnly) {
    std::string email_domains;
    if (net::GetValueForKeyInQuery(
            current_url, credential_provider::kEmailDomainsSigninPromoParameter,
            &email_domains)) {
      std::vector<std::string> all_email_domains =
          GetEmailDomainsFromParameter(email_domains);
      if (all_email_domains.size() == 1)
        params.Set("emailDomain", all_email_domains[0]);
    }

    std::string show_tos;
    if (net::GetValueForKeyInQuery(
            current_url, credential_provider::kShowTosSwitch, &show_tos)) {
      if (!show_tos.empty())
        params.Set("showTos", show_tos);
    }

    // Prevent opening a new window if the embedded page fails to load.
    // This will keep the user from being able to access a fully functional
    // Chrome window in incognito mode.
    params.Set("dontResizeNonEmbeddedPages", true);

    // Scrape the SAML password if possible.
    params.Set("extractSamlPasswordAttributes", true);

    GURL windows_url = GaiaUrls::GetInstance()->embedded_setup_windows_url();
    // Redirect to specified gaia endpoint path for GCPW:
    std::string windows_endpoint_path = windows_url.path().substr(1);
    // Redirect to specified gaia endpoint path for GCPW:
    std::string gcpw_endpoint_path;
    if (net::GetValueForKeyInQuery(
            current_url, credential_provider::kGcpwEndpointPathPromoParameter,
            &gcpw_endpoint_path)) {
      windows_endpoint_path = gcpw_endpoint_path;
    }
    params.Set("gaiaPath", windows_endpoint_path);
  }
#endif

  std::string flow;
  switch (reason) {
    case HandlerSigninReason::kReauthentication:
      flow = "reauth";
      break;
    case HandlerSigninReason::kForcedSigninPrimaryAccount:
      flow = "enterprisefsi";
      break;
    case HandlerSigninReason::kFetchLstOnly: {
#if BUILDFLAG(IS_WIN)
      // Treat a sign in request that specifies a gaia id that must be validated
      // as a reauth request. We only get a gaia id from GCPW when trying to
      // reauth an existing user on the system.
      std::string validate_gaia_id;
      net::GetValueForKeyInQuery(
          current_url, credential_provider::kValidateGaiaIdSigninPromoParameter,
          &validate_gaia_id);
      if (validate_gaia_id.empty()) {
        flow = "signin";
      } else {
        flow = "reauth";
      }
#else
      flow = "signin";
#endif
    } break;
  }
  params.Set("flow", flow);
}

void InlineLoginHandlerImpl::CompleteLogin(const CompleteLoginParams& params) {
  content::WebContents* contents = web_ui()->GetWebContents();
  const GURL& current_url = contents->GetLastCommittedURL();

  if (params.skip_for_now) {
    SyncSetupFailed();
    return;
  }

  // This value exists only for webview sign in.
  if (params.trusted_found)
    confirm_untrusted_signin_ = !params.trusted_value;

  DCHECK(!params.email.empty());
  DCHECK(!params.gaia_id.empty());
  DCHECK(!params.auth_code.empty());

  content::StoragePartition* partition =
      signin::GetSigninPartition(contents->GetBrowserContext());

  HandlerSigninReason reason = GetHandlerSigninReason(current_url);
  Profile* profile = Profile::FromWebUI(web_ui());

  bool force_sign_in_with_usermanager = false;
  base::FilePath path;
  if (reason != HandlerSigninReason::kFetchLstOnly &&
      HasFromProfilePickerURLParameter(current_url)) {
    DCHECK(reason == HandlerSigninReason::kForcedSigninPrimaryAccount ||
           reason == HandlerSigninReason::kReauthentication);
    DCHECK(signin_util::IsForceSigninEnabled());

    force_sign_in_with_usermanager = true;
    path = profile->GetPath();
  }

  FinishCompleteLogin(
      FinishCompleteLoginParams(
          this, partition, current_url, path, confirm_untrusted_signin_,
          params.email, params.gaia_id, params.password, params.auth_code,
          force_sign_in_with_usermanager),
      profile);
}

InlineLoginHandlerImpl::FinishCompleteLoginParams::FinishCompleteLoginParams(
    InlineLoginHandlerImpl* handler,
    content::StoragePartition* partition,
    const GURL& url,
    const base::FilePath& profile_path,
    bool confirm_untrusted_signin,
    const std::string& email,
    const std::string& gaia_id,
    const std::string& password,
    const std::string& auth_code,
    bool is_force_sign_in_with_usermanager)
    : handler(handler),
      partition(partition),
      url(url),
      profile_path(profile_path),
      confirm_untrusted_signin(confirm_untrusted_signin),
      email(email),
      gaia_id(gaia_id),
      password(password),
      auth_code(auth_code),
      is_force_sign_in_with_usermanager(is_force_sign_in_with_usermanager) {}

InlineLoginHandlerImpl::FinishCompleteLoginParams::FinishCompleteLoginParams(
    const FinishCompleteLoginParams& other) = default;

InlineLoginHandlerImpl::FinishCompleteLoginParams::
    ~FinishCompleteLoginParams() {}

// static
void InlineLoginHandlerImpl::FinishCompleteLogin(
    const FinishCompleteLoginParams& params,
    Profile* profile) {
  DCHECK(params.handler);
  HandlerSigninReason reason = GetHandlerSigninReason(params.url);

  std::string default_email;
  net::GetValueForKeyInQuery(params.url, "email", &default_email);
  std::string validate_email;
  net::GetValueForKeyInQuery(params.url, "validateEmail", &validate_email);

#if BUILDFLAG(IS_WIN)
  if (reason == HandlerSigninReason::kFetchLstOnly) {
    std::string validate_gaia_id;
    net::GetValueForKeyInQuery(
        params.url, credential_provider::kValidateGaiaIdSigninPromoParameter,
        &validate_gaia_id);
    std::string email_domains;
    net::GetValueForKeyInQuery(
        params.url, credential_provider::kEmailDomainsSigninPromoParameter,
        &email_domains);
    credential_provider::UiExitCodes exit_code = ValidateSigninEmail(
        validate_gaia_id, email_domains, params.email, params.gaia_id);
    if (exit_code != credential_provider::kUiecSuccess) {
      params.handler->HandleLoginError(
          SigninUIError::FromCredentialProviderUiExitCode(params.email,
                                                          exit_code));
      return;
    } else {
      // Validation has already been done for GCPW, so clear the validate
      // argument so it doesn't validate again. GCPW validation allows the
      // signin email to not match the email given in the request url if the
      // gaia id of the signin email matches the one given in the request url.
      validate_email.clear();
    }
  }
#endif

  // When doing a SAML sign in, this email check may result in a false positive.
  // This happens when the user types one email address in the gaia sign in
  // page, but signs in to a different account in the SAML sign in page.
  if (validate_email == "1" && !default_email.empty()) {
    if (!gaia::AreEmailsSame(params.email, default_email)) {
      params.handler->HandleLoginError(
          SigninUIError::WrongReauthAccount(params.email, default_email));
      return;
    }
  }

  SigninUIError can_offer_error = SigninUIError::Ok();
  switch (reason) {
    case HandlerSigninReason::kReauthentication:
    case HandlerSigninReason::kForcedSigninPrimaryAccount:
      can_offer_error = CanOfferSignin(profile, params.gaia_id, params.email);
      break;
    case HandlerSigninReason::kFetchLstOnly:
      break;
  }

  if (!can_offer_error.IsOk()) {
    params.handler->HandleLoginError(can_offer_error);
    return;
  }

  AboutSigninInternals* about_signin_internals =
      AboutSigninInternalsFactory::GetForProfile(profile);
  if (about_signin_internals)
    about_signin_internals->OnAuthenticationResultReceived("Successful");

  std::string signin_scoped_device_id =
      GetSigninScopedDeviceIdForProfile(profile);

  // InlineSigninHelper will delete itself.
  new InlineSigninHelper(
      params.handler->GetWeakPtr(),
      params.partition->GetURLLoaderFactoryForBrowserProcess(), profile,
      params.url, params.email, params.gaia_id, params.password,
      params.auth_code, signin_scoped_device_id,
      params.confirm_untrusted_signin,
      params.is_force_sign_in_with_usermanager);

  // If opened from user manager to unlock a profile, make sure the user manager
  // is closed and that the profile is marked as unlocked.
  if (reason != HandlerSigninReason::kFetchLstOnly &&
      !params.is_force_sign_in_with_usermanager) {
    UnlockProfileAndHideLoginUI(params.profile_path, params.handler);
  }
}

void InlineLoginHandlerImpl::HandleLoginError(const SigninUIError& error) {
  content::WebContents* contents = web_ui()->GetWebContents();
  const GURL& current_url = contents->GetLastCommittedURL();
  HandlerSigninReason reason = GetHandlerSigninReason(current_url);

  if (reason == HandlerSigninReason::kFetchLstOnly) {
    base::Value::Dict error_value;
#if BUILDFLAG(IS_WIN)
    // If the error contains an integer error code, send it as part of the
    // result.
    if (error.type() ==
        SigninUIError::Type::kFromCredentialProviderUiExitCode) {
      error_value.Set(credential_provider::kKeyExitCode,
                      base::Value(error.credential_provider_exit_code()));
    }
#endif
    SendLSTFetchResultsMessage(base::Value(std::move(error_value)));
    return;
  }
  SyncSetupFailed();

  if (!error.IsOk()) {
    Browser* browser = GetDesktopBrowser();
    Profile* profile = Profile::FromWebUI(web_ui());
    LoginUIServiceFactory::GetForProfile(profile)->DisplayLoginResult(
        browser, error, HasFromProfilePickerURLParameter(current_url));
  }
}

void InlineLoginHandlerImpl::SendLSTFetchResultsMessage(
    const base::Value& arg) {
  if (IsJavascriptAllowed())
    FireWebUIListener("send-lst-fetch-results", arg);
}

Browser* InlineLoginHandlerImpl::GetDesktopBrowser() {
  Browser* browser = chrome::FindBrowserWithTab(web_ui()->GetWebContents());
  if (!browser)
    browser = chrome::FindLastActiveWithProfile(Profile::FromWebUI(web_ui()));
  return browser;
}

void InlineLoginHandlerImpl::SyncSetupFailed() {
  content::WebContents* contents = web_ui()->GetWebContents();

  if (contents->GetController().GetPendingEntry()) {
    // Do nothing if a navigation is pending, since this call can be triggered
    // from DidStartLoading. This avoids deleting the pending entry while we are
    // still navigating to it. See crbug/346632.
    return;
  }

  // Redirect to NTP.
  GURL url(chrome::kChromeUINewTabURL);
  content::OpenURLParams params(url, content::Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
                                ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false);
  contents->OpenURL(params, /*navigation_handle_callback=*/{});
}
