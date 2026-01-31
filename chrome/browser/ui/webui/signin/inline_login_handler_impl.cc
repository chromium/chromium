// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/inline_login_handler_impl.h"

#include <stddef.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/not_fatal_until.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/about_signin_internals_factory.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
#include "chrome/common/url_constants.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/grit/branded_strings.h"
#include "components/signin/core/browser/about_signin_internals.h"
#include "components/signin/public/base/signin_metrics.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// This struct exists to pass parameters to the FinishCompleteLogin() method,
// since the base::BindRepeating() call does not support this many template
// args.
struct FinishCompleteLoginParams {
 public:
  FinishCompleteLoginParams(InlineLoginHandlerImpl* handler,
                            content::StoragePartition* partition,
                            const GURL& url,
                            const std::string& email,
                            const GaiaId& gaia_id,
                            const std::string& password,
                            const std::string& auth_code)
      : handler(handler),
        partition(partition),
        url(url),
        email(email),
        gaia_id(gaia_id),
        password(password),
        auth_code(auth_code) {}
  FinishCompleteLoginParams(const FinishCompleteLoginParams& other) = default;
  ~FinishCompleteLoginParams() = default;

  // Pointer to WebUI handler.  May be nullptr.
  raw_ptr<InlineLoginHandlerImpl> handler;
  // The isolate storage partition containing sign in cookies.
  raw_ptr<content::StoragePartition> partition;
  // URL of sign in containing parameters such as email, source, etc.
  GURL url;
  // Email address of the account used to sign in.
  std::string email;
  // Obfustcated gaia id of the account used to sign in.
  GaiaId gaia_id;
  // Password of the account used to sign in.
  std::string password;
  // Authentication code used to exchange for a login scoped refresh token
  // for the account used to sign in.  Used only with password separated
  // signin flow.
  std::string auth_code;
};

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
    const GaiaId& signin_gaia_id) {
  if (!gaia_id_parameter.empty() &&
      !base::EqualsCaseInsensitiveASCII(gaia_id_parameter,
                                        signin_gaia_id.ToString())) {
    return credential_provider::kUiecEMailMissmatch;
  }

  if (email_domains_parameter.empty()) {
    return credential_provider::kUiecSuccess;
  }

  std::vector<std::string> all_email_domains =
      GetEmailDomainsFromParameter(email_domains_parameter);
  std::string email_domain = gaia::ExtractDomainName(signin_email);

  return std::ranges::contains(all_email_domains, email_domain)
             ? credential_provider::kUiecSuccess
             : credential_provider::kUiecInvalidEmailDomain;
}

void FinishCompleteLogin(const FinishCompleteLoginParams& params,
                         Profile* profile) {
  DCHECK(params.handler);
  CHECK_EQ(signin::GetSigninReasonForEmbeddedPromoURL(params.url),
           signin_metrics::Reason::kFetchLstOnly);

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
  }

  AboutSigninInternals* about_signin_internals =
      AboutSigninInternalsFactory::GetForProfile(profile);
  if (about_signin_internals) {
    about_signin_internals->OnAuthenticationResultReceived("Successful");
  }

  std::string signin_scoped_device_id =
      GetSigninScopedDeviceIdForProfile(profile);

  // InlineSigninHelper will delete itself.
  new InlineSigninHelper(
      params.handler->GetWeakPtr(),
      params.partition->GetURLLoaderFactoryForBrowserProcess(), profile,
      params.url, params.email, params.gaia_id, params.password,
      params.auth_code, signin_scoped_device_id);
}

}  // namespace

InlineSigninHelper::InlineSigninHelper(
    base::WeakPtr<InlineLoginHandlerImpl> handler,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    Profile* profile,
    const GURL& current_url,
    const std::string& email,
    const GaiaId& gaia_id,
    const std::string& password,
    const std::string& auth_code,
    const std::string& signin_scoped_device_id)
    : gaia_auth_fetcher_(this, gaia::GaiaSource::kChrome, url_loader_factory),
      handler_(handler),
      profile_(profile),
      current_url_(current_url),
      email_(email),
      gaia_id_(gaia_id),
      password_(password),
      auth_code_(auth_code) {
  DCHECK(profile_);
  DCHECK(!email_.empty());
  DCHECK(!auth_code_.empty());
  DCHECK(handler);

  gaia_auth_fetcher_.StartAuthCodeForOAuth2TokenExchangeWithDeviceId(
      auth_code_, signin_scoped_device_id);
}

InlineSigninHelper::~InlineSigninHelper() = default;

void InlineSigninHelper::OnClientOAuthSuccess(const ClientOAuthResult& result) {
  CHECK_EQ(signin::GetSigninReasonForEmbeddedPromoURL(current_url_),
           signin_metrics::Reason::kFetchLstOnly);
  std::string json_retval;
  base::DictValue args;
  args.Set(credential_provider::kKeyEmail, base::Value(email_));
  args.Set(credential_provider::kKeyPassword, base::Value(password_));
  args.Set(credential_provider::kKeyId, base::Value(gaia_id_.ToString()));
  args.Set(credential_provider::kKeyRefreshToken,
           base::Value(result.refresh_token));
  args.Set(credential_provider::kKeyAccessToken,
           base::Value(result.access_token));

  handler_->SendLSTFetchResultsMessage(base::Value(std::move(args)));
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                this);
}

void InlineSigninHelper::OnClientOAuthFailure(
    const GoogleServiceAuthError& error) {
  if (handler_) {
    handler_->HandleLoginError(
        SigninUIError::FromGoogleServiceAuthError(email_, error));
  }

  CHECK_EQ(signin::GetSigninReasonForEmbeddedPromoURL(current_url_),
           signin_metrics::Reason::kFetchLstOnly);

  AboutSigninInternals* about_signin_internals =
      AboutSigninInternalsFactory::GetForProfile(profile_);
  about_signin_internals->OnRefreshTokenReceived("Failure");

  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                this);
}

InlineLoginHandlerImpl::InlineLoginHandlerImpl() = default;

InlineLoginHandlerImpl::~InlineLoginHandlerImpl() = default;

// static
void InlineLoginHandlerImpl::SetExtraInitParams(base::DictValue& params) {
  params.Set("service", "chromiumsync");

  const GURL& url = GaiaUrls::GetInstance()->embedded_signin_url();
  params.Set("clientId", GaiaUrls::GetInstance()->oauth2_chrome_client_id());
  params.Set("gaiaPath", url.GetPath().substr(1));

  content::WebContents* contents = web_ui()->GetWebContents();
  const GURL& current_url = contents->GetLastCommittedURL();
  CHECK_EQ(signin::GetSigninReasonForEmbeddedPromoURL(current_url),
           signin_metrics::Reason::kFetchLstOnly);
  std::string email_domains;
  if (net::GetValueForKeyInQuery(
          current_url, credential_provider::kEmailDomainsSigninPromoParameter,
          &email_domains)) {
    std::vector<std::string> all_email_domains =
        GetEmailDomainsFromParameter(email_domains);
    if (all_email_domains.size() == 1) {
      params.Set("emailDomain", all_email_domains[0]);
    }
  }

  std::string show_tos;
  if (net::GetValueForKeyInQuery(
          current_url, credential_provider::kShowTosSwitch, &show_tos)) {
    if (!show_tos.empty()) {
      params.Set("showTos", show_tos);
    }
  }

  // Prevent opening a new window if the embedded page fails to load.
  // This will keep the user from being able to access a fully functional
  // Chrome window in incognito mode.
  params.Set("dontResizeNonEmbeddedPages", true);

  // Scrape the SAML password if possible.
  params.Set("extractSamlPasswordAttributes", true);

  GURL windows_url = GaiaUrls::GetInstance()->embedded_setup_windows_url();
  // Redirect to specified gaia endpoint path for GCPW:
  std::string windows_endpoint_path = windows_url.GetPath().substr(1);
  // Redirect to specified gaia endpoint path for GCPW:
  std::string gcpw_endpoint_path;
  if (net::GetValueForKeyInQuery(
          current_url, credential_provider::kGcpwEndpointPathPromoParameter,
          &gcpw_endpoint_path)) {
    windows_endpoint_path = gcpw_endpoint_path;
  }
  params.Set("gaiaPath", windows_endpoint_path);

  std::string flow;
  // Treat a sign in request that specifies a gaia id that must be validated as
  // a reauth request. We only get a gaia id from GCPW when trying to reauth an
  // existing user on the system.
  std::string validate_gaia_id;
  net::GetValueForKeyInQuery(
      current_url, credential_provider::kValidateGaiaIdSigninPromoParameter,
      &validate_gaia_id);
  if (validate_gaia_id.empty()) {
    flow = "signin";
  } else {
    flow = "reauth";
  }
  params.Set("flow", flow);
}

void InlineLoginHandlerImpl::CompleteLogin(const CompleteLoginParams& params) {
  if (params.skip_for_now) {
    // TODO(crbug.com/381231566): The assumption is that this part of the flow
    // was only reachable by force signin flow, which is mostly cleaned up.
    // Adding `base::NotFatalUntil` for few milestones to ensure the assumption.
    CHECK(false, base::NotFatalUntil::M152);
    return;
  }

  DCHECK(!params.email.empty());
  DCHECK(!params.gaia_id.empty());
  DCHECK(!params.auth_code.empty());

  content::WebContents* contents = web_ui()->GetWebContents();
  const GURL& current_url = contents->GetLastCommittedURL();
  content::StoragePartition* partition =
      signin::GetSigninPartition(contents->GetBrowserContext());
  Profile* profile = Profile::FromWebUI(web_ui());
  FinishCompleteLogin(FinishCompleteLoginParams(
                          this, partition, current_url, params.email,
                          params.gaia_id, params.password, params.auth_code),
                      profile);
}

void InlineLoginHandlerImpl::HandleLoginError(const SigninUIError& error) {
  content::WebContents* contents = web_ui()->GetWebContents();
  const GURL& current_url = contents->GetLastCommittedURL();
  CHECK_EQ(signin::GetSigninReasonForEmbeddedPromoURL(current_url),
           signin_metrics::Reason::kFetchLstOnly);

  base::DictValue error_value;
  // If the error contains an integer error code, send it as part of the
  // result.
  if (error.type() == SigninUIError::Type::kFromCredentialProviderUiExitCode) {
    error_value.Set(credential_provider::kKeyExitCode,
                    base::Value(error.credential_provider_exit_code()));
  }
  SendLSTFetchResultsMessage(base::Value(std::move(error_value)));
}

void InlineLoginHandlerImpl::SendLSTFetchResultsMessage(
    const base::Value& arg) {
  if (IsJavascriptAllowed()) {
    FireWebUIListener("send-lst-fetch-results", arg);
  }
}
