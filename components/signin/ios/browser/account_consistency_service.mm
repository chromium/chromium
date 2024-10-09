// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/ios/browser/account_consistency_service.h"

#import <WebKit/WebKit.h>

#import "base/apple/foundation_util.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#import "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/google/core/common/google_util.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/chrome_connected_header_helper.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/ios/browser/features.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "ios/web/common/web_view_creation_util.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/web_state.h"
#include "ios/web/public/web_state_observer.h"
#include "net/base/apple/url_conversions.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/canonical_cookie.h"
#include "url/gurl.h"

namespace {

// The validity of the Gaia cookie on the Google domain is one hour to
// ensure that Mirror account consistency is respected in light of the more
// restrictive Intelligent Tracking Prevention (ITP) guidelines in iOS 14
// that may remove or invalidate Gaia cookies on the Google domain.
constexpr base::TimeDelta kDelayThresholdToUpdateGaiaCookie = base::Hours(1);

const char* kGoogleUrl = "https://google.com";
const char* kYoutubeUrl = "https://youtube.com";
const char* kGaiaDomain = "accounts.google.com";

// Returns the registered, organization-identifying host, but no subdomains,
// from the given GURL. Returns an empty string if the GURL is invalid.
static std::string GetDomainFromUrl(const GURL& url) {
  if (gaia::HasGaiaSchemeHostPort(url)) {
    return kGaiaDomain;
  }
  return net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
}

// The Gaia cookie state on navigation for a signed-in Chrome user.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GaiaCookieStateOnSignedInNavigation {
  kGaiaCookiePresentOnNavigation = 0,
  kGaiaCookieAbsentOnGoogleAssociatedDomainNavigation = 1,
  kGaiaCookieAbsentOnAddSessionNavigation = 2,
  kGaiaCookieRestoredOnShowInfobar = 3,
  kMaxValue = kGaiaCookieRestoredOnShowInfobar
};

// Records the state of Gaia cookies for a navigation in UMA histogram.
void LogIOSGaiaCookiesState(GaiaCookieStateOnSignedInNavigation state) {
  base::UmaHistogramEnumeration("Signin.IOSGaiaCookieStateOnSignedInNavigation",
                                state);
}

// Allows for manual testing by reducing the polling interval for verifying the
// existence of the GAIA cookie.
base::TimeDelta GetDelayThresholdToUpdateGaiaCookie() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(
          signin::kDelayThresholdMinutesToUpdateGaiaCookie)) {
    std::string delayString = command_line->GetSwitchValueASCII(
        signin::kDelayThresholdMinutesToUpdateGaiaCookie);
    int commandLineDelay = 0;
    if (base::StringToInt(delayString, &commandLineDelay)) {
      return base::Minutes(commandLineDelay);
    }
  }
  return kDelayThresholdToUpdateGaiaCookie;
}

}  // namespace

// WebStatePolicyDecider that monitors the HTTP headers on Gaia responses,
// reacting on the X-Chrome-Manage-Accounts header and notifying its delegate.
// It also notifies the AccountConsistencyService of domains it should add the
// CHROME_CONNECTED cookie to.
class AccountConsistencyService::AccountConsistencyHandler
    : public web::WebStatePolicyDecider,
      public web::WebStateObserver {
 public:
  AccountConsistencyHandler(web::WebState* web_state,
                            AccountConsistencyService* service,
                            AccountReconcilor* account_reconcilor,
                            signin::IdentityManager* identity_manager,
                            ManageAccountsDelegate* delegate);

  void WebStateDestroyed(web::WebState* web_state) override;

  AccountConsistencyHandler(const AccountConsistencyHandler&) = delete;
  AccountConsistencyHandler& operator=(const AccountConsistencyHandler&) =
      delete;

 private:
  // web::WebStateObserver override.
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;

  // web::WebStatePolicyDecider override.
  void ShouldAllowRequest(
      NSURLRequest* request,
      web::WebStatePolicyDecider::RequestInfo request_info,
      web::WebStatePolicyDecider::PolicyDecisionCallback callback) override;
  // Decides on navigation corresponding to |response| whether the navigation
  // should continue and updates authentication cookies on Google domains.
  void ShouldAllowResponse(
      NSURLResponse* response,
      web::WebStatePolicyDecider::ResponseInfo response_info,
      web::WebStatePolicyDecider::PolicyDecisionCallback callback) override;
  void WebStateDestroyed() override;

  // Handles the AddAccount request depending on |has_cookie_changed|.
  void HandleAddAccountRequest(GURL url, BOOL has_cookie_changed);

  // The consistency web sign-in needs to be shown once the page is loaded.
  // It is required to avoid having the keyboard showing up on top of the web
  // sign-in dialog.
  bool show_consistency_web_signin_ = false;
  raw_ptr<AccountConsistencyService> account_consistency_service_;  // Weak.
  raw_ptr<AccountReconcilor> account_reconcilor_;                   // Weak.
  raw_ptr<signin::IdentityManager> identity_manager_;
  raw_ptr<web::WebState> web_state_;
  raw_ptr<ManageAccountsDelegate> delegate_;  // Weak.
  base::WeakPtrFactory<AccountConsistencyHandler> weak_ptr_factory_;
};

AccountConsistencyService::AccountConsistencyHandler::AccountConsistencyHandler(
    web::WebState* web_state,
    AccountConsistencyService* service,
    AccountReconcilor* account_reconcilor,
    signin::IdentityManager* identity_manager,
    ManageAccountsDelegate* delegate)
    : web::WebStatePolicyDecider(web_state),
      account_consistency_service_(service),
      account_reconcilor_(account_reconcilor),
      identity_manager_(identity_manager),
      web_state_(web_state),
      delegate_(delegate),
      weak_ptr_factory_(this) {
  web_state->AddObserver(this);
}

void AccountConsistencyService::AccountConsistencyHandler::ShouldAllowRequest(
    NSURLRequest* request,
    web::WebStatePolicyDecider::RequestInfo request_info,
    web::WebStatePolicyDecider::PolicyDecisionCallback callback) {
  GURL url = net::GURLWithNSURL(request.URL);
  if (signin::IsUrlEligibleForMirrorCookie(url) &&
      identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    // CHROME_CONNECTED cookies are added asynchronously on google.com and
    // youtube.com domains when Chrome detects that the user is signed-in. By
    // continuing to fulfill the navigation once the cookie request is sent,
    // Chrome adopts a best-effort strategy for signing the user into the web if
    // necessary.
    account_consistency_service_->AddChromeConnectedCookies();
  }
  std::move(callback).Run(PolicyDecision::Allow());
}

void AccountConsistencyService::AccountConsistencyHandler::ShouldAllowResponse(
    NSURLResponse* response,
    web::WebStatePolicyDecider::ResponseInfo response_info,
    web::WebStatePolicyDecider::PolicyDecisionCallback callback) {
  NSHTTPURLResponse* http_response =
      base::apple::ObjCCast<NSHTTPURLResponse>(response);
  if (!http_response) {
    std::move(callback).Run(PolicyDecision::Allow());
    return;
  }

  GURL url = net::GURLWithNSURL(http_response.URL);
  // User is showing intent to navigate to a Google-owned domain. Set GAIA and
  // CHROME_CONNECTED cookies if the user is signed in (this is filtered in
  // ChromeConnectedHelper).
  if (signin::IsUrlEligibleForMirrorCookie(url)) {
    account_consistency_service_->SetChromeConnectedCookieWithUrls(
        {url, GURL(kGoogleUrl)});
  }

  if (!gaia::HasGaiaSchemeHostPort(url)) {
    std::move(callback).Run(PolicyDecision::Allow());
    return;
  }

  NSString* manage_accounts_header = [[http_response allHeaderFields]
      objectForKey:
          [NSString stringWithUTF8String:signin::kChromeManageAccountsHeader]];
  if (!manage_accounts_header) {
    // Header that detects whether a user has been prompted to enter their
    // credentials on a Gaia sign-on page.
    NSString* x_autologin_header = [[http_response allHeaderFields]
        objectForKey:[NSString stringWithUTF8String:signin::kAutoLoginHeader]];
    if (x_autologin_header) {
      show_consistency_web_signin_ = true;
    }
    std::move(callback).Run(PolicyDecision::Allow());
    return;
  }

  signin::ManageAccountsParams params = signin::BuildManageAccountsParams(
      base::SysNSStringToUTF8(manage_accounts_header));

  account_reconcilor_->OnReceivedManageAccountsResponse(params.service_type);

  switch (params.service_type) {
    case signin::GAIA_SERVICE_TYPE_INCOGNITO: {
      GURL continue_url = GURL(params.continue_url);
      DLOG_IF(ERROR, !params.continue_url.empty() && !continue_url.is_valid())
          << "Invalid continuation URL: \"" << continue_url << "\"";
      if (delegate_)
        delegate_->OnGoIncognito(continue_url);
      break;
    }
    case signin::GAIA_SERVICE_TYPE_SIGNUP:
    case signin::GAIA_SERVICE_TYPE_ADDSESSION:
      // This situation is only possible if the all cookies have been deleted by
      // ITP restrictions and Chrome has not triggered a cookie refresh.
      if (identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
        LogIOSGaiaCookiesState(GaiaCookieStateOnSignedInNavigation::
                                   kGaiaCookieAbsentOnAddSessionNavigation);
        GURL continue_url = GURL(params.continue_url);
        DLOG_IF(ERROR, !params.continue_url.empty() && !continue_url.is_valid())
            << "Invalid continuation URL: \"" << continue_url << "\"";
        if (account_consistency_service_->RestoreGaiaCookies(base::BindOnce(
                &AccountConsistencyHandler::HandleAddAccountRequest,
                weak_ptr_factory_.GetWeakPtr(), continue_url))) {
          // Continue URL will be processed in a callback once Gaia cookies
          // have been restored.
          return;
        }
      }
      if (delegate_)
        delegate_->OnAddAccount();
      break;
    case signin::GAIA_SERVICE_TYPE_SIGNOUT:
    case signin::GAIA_SERVICE_TYPE_DEFAULT:
      if (delegate_)
        delegate_->OnManageAccounts();
      break;
    case signin::GAIA_SERVICE_TYPE_NONE:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  // WKWebView loads a blank page even if the response code is 204
  // ("No Content"). http://crbug.com/368717
  //
  // Manage accounts responses are handled via native UI. Abort this request
  // for the following reasons:
  // * Avoid loading a blank page in WKWebView.
  // * Avoid adding this request to history.
  std::move(callback).Run(PolicyDecision::Cancel());
}

void AccountConsistencyService::AccountConsistencyHandler::
    HandleAddAccountRequest(GURL url, BOOL has_cookie_changed) {
  if (!has_cookie_changed) {
    // If the cookies on the device did not need to be updated then the user
    // is not in an inconsistent state (where the identities on the device
    // are different than those on the web). Fallback to asking the user to
    // add an account.
    if (delegate_)
      delegate_->OnAddAccount();
    return;
  }
  web_state_->OpenURL(web::WebState::OpenURLParams(
      url, web::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false));
  if (delegate_)
    delegate_->OnRestoreGaiaCookies();
  LogIOSGaiaCookiesState(
      GaiaCookieStateOnSignedInNavigation::kGaiaCookieRestoredOnShowInfobar);
}

void AccountConsistencyService::AccountConsistencyHandler::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  const GURL& url = web_state->GetLastCommittedURL();
  if (load_completion_status == web::PageLoadCompletionStatus::FAILURE ||
      !google_util::IsGoogleDomainUrl(
          url, google_util::ALLOW_SUBDOMAIN,
          google_util::DISALLOW_NON_STANDARD_PORTS)) {
    return;
  }

  if (delegate_ && show_consistency_web_signin_ &&
      gaia::HasGaiaSchemeHostPort(url)) {
    delegate_->OnShowConsistencyPromo(url, web_state);
  }
  show_consistency_web_signin_ = false;
}

void AccountConsistencyService::AccountConsistencyHandler::WebStateDestroyed(
    web::WebState* web_state) {}

void AccountConsistencyService::AccountConsistencyHandler::WebStateDestroyed() {
  account_consistency_service_->RemoveWebStateHandler(web_state());
}

AccountConsistencyService::AccountConsistencyService(
    CookieManagerCallback cookie_manager_cb,
    AccountReconcilor* account_reconcilor,
    scoped_refptr<content_settings::CookieSettings> cookie_settings,
    signin::IdentityManager* identity_manager)
    : cookie_manager_cb_(std::move(cookie_manager_cb)),
      account_reconcilor_(account_reconcilor),
      cookie_settings_(cookie_settings),
      identity_manager_(identity_manager),
      active_cookie_manager_requests_for_testing_(0) {
  DCHECK(!cookie_manager_cb_.is_null());
  identity_manager_->AddObserver(this);
  if (identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    AddChromeConnectedCookies();
  } else {
    RemoveAllChromeConnectedCookies(base::OnceClosure());
  }
}

AccountConsistencyService::~AccountConsistencyService() = default;

BOOL AccountConsistencyService::RestoreGaiaCookies(
    base::OnceCallback<void(BOOL)> cookies_restored_callback) {
  // We currently enforce a time threshold to update the Gaia cookie
  // for signed-in users to prevent calling the expensive method
  // |GetAllCookies| in the cookie manager.
  if (last_gaia_cookie_update_time_.is_null() ||
      base::Time::Now() - last_gaia_cookie_update_time_ >
          GetDelayThresholdToUpdateGaiaCookie()) {
    network::mojom::CookieManager* cookie_manager = cookie_manager_cb_.Run();
    cookie_manager->GetCookieList(
        GaiaUrls::GetInstance()->secure_google_url(),
        net::CookieOptions::MakeAllInclusive(),
        net::CookiePartitionKeyCollection::Todo(),
        base::BindOnce(
            &AccountConsistencyService::TriggerGaiaCookieChangeIfDeleted,
            base::Unretained(this), std::move(cookies_restored_callback)));
    last_gaia_cookie_update_time_ = base::Time::Now();
    return YES;
  }
  return NO;
}

void AccountConsistencyService::TriggerGaiaCookieChangeIfDeleted(
    base::OnceCallback<void(BOOL)> cookies_restored_callback,
    const net::CookieAccessResultList& cookie_list,
    const net::CookieAccessResultList& unused_excluded_cookies) {
  gaia_cookies_restored_callbacks_.push_back(
      std::move(cookies_restored_callback));

  for (const auto& cookie : cookie_list) {
    // There may be other cookies besides `kGaiaSigninCookieName` that are
    // required. However these cookies are not specified by the server, and thus
    // Chrome cannot monitor them. We assume here that ITP will remove all the
    // cookies fom the domain at once, and so it is sufficient to monitor only
    // one cookie.
    if (cookie.cookie.Name() == GaiaConstants::kGaiaSigninCookieName) {
      LogIOSGaiaCookiesState(
          GaiaCookieStateOnSignedInNavigation::kGaiaCookiePresentOnNavigation);
      RunGaiaCookiesRestoredCallbacks(/*has_cookie_changed=*/NO);
      return;
    }
  }

  // The SAPISID cookie may have been deleted previous to this update due to
  // ITP restrictions marking Google domains as potential trackers.
  LogIOSGaiaCookiesState(
      GaiaCookieStateOnSignedInNavigation::
          kGaiaCookieAbsentOnGoogleAssociatedDomainNavigation);

  // Re-generate cookie to ensure that the user is properly signed in.
  identity_manager_->GetAccountsCookieMutator()->ForceTriggerOnCookieChange();
}

void AccountConsistencyService::RunGaiaCookiesRestoredCallbacks(
    BOOL has_cookie_changed) {
  std::vector<base::OnceCallback<void(BOOL)>> callbacks;
  std::swap(gaia_cookies_restored_callbacks_, callbacks);
  for (base::OnceCallback<void(BOOL)>& callback : callbacks) {
    std::move(callback).Run(has_cookie_changed);
  }
}

void AccountConsistencyService::SetWebStateHandler(
    web::WebState* web_state,
    ManageAccountsDelegate* delegate) {
  DCHECK(!is_shutdown_) << "SetWebStateHandler called after Shutdown";
  DCHECK(handlers_map_.find(web_state) == handlers_map_.end());
  handlers_map_.insert(std::make_pair(
      web_state,
      std::make_unique<AccountConsistencyHandler>(
          web_state, this, account_reconcilor_, identity_manager_, delegate)));
}

void AccountConsistencyService::RemoveWebStateHandler(
    web::WebState* web_state) {
  DCHECK(!is_shutdown_) << "RemoveWebStateHandler called after Shutdown";
  auto iter = handlers_map_.find(web_state);
  DCHECK(iter != handlers_map_.end());

  std::unique_ptr<AccountConsistencyHandler> handler = std::move(iter->second);
  handlers_map_.erase(iter);

  web_state->RemoveObserver(handler.get());
}

void AccountConsistencyService::RemoveAllChromeConnectedCookies(
    base::OnceClosure callback) {
  network::mojom::CookieManager* cookie_manager = cookie_manager_cb_.Run();

  network::mojom::CookieDeletionFilterPtr filter =
      network::mojom::CookieDeletionFilter::New();
  filter->cookie_name = signin::kChromeConnectedCookieName;

  ++active_cookie_manager_requests_for_testing_;
  cookie_manager->DeleteCookies(
      std::move(filter),
      base::BindOnce(&AccountConsistencyService::OnDeleteCookiesFinished,
                     base::Unretained(this), std::move(callback)));
}

void AccountConsistencyService::OnDeleteCookiesFinished(
    base::OnceClosure callback,
    uint32_t unused_num_cookies_deleted) {
  --active_cookie_manager_requests_for_testing_;
  if (!callback.is_null()) {
    std::move(callback).Run();
  }
}

void AccountConsistencyService::SetChromeConnectedCookieWithUrls(
    const std::vector<GURL>& urls) {
  for (const GURL& url : urls) {
    SetChromeConnectedCookieWithUrl(url);
  }
}

void AccountConsistencyService::Shutdown() {
  DCHECK(handlers_map_.empty()) << "Handlers not unregistered at shutdown";
  identity_manager_->RemoveObserver(this);
  is_shutdown_ = true;
}

void AccountConsistencyService::SetChromeConnectedCookieWithUrl(
    const GURL& url) {
  const std::string domain = GetDomainFromUrl(url);
  std::string cookie_value = signin::BuildMirrorRequestCookieIfPossible(
      url,
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia,
      signin::AccountConsistencyMethod::kMirror, cookie_settings_.get(),
      signin::PROFILE_MODE_DEFAULT);
  if (cookie_value.empty()) {
    return;
  }

  std::unique_ptr<net::CanonicalCookie> cookie =
      net::CanonicalCookie::CreateSanitizedCookie(
          url,
          /*name=*/signin::kChromeConnectedCookieName, cookie_value,
          /*domain=*/domain,
          /*path=*/std::string(),
          /*creation_time=*/base::Time::Now(),
          // Create expiration date of Now+2y to roughly follow the SAPISID
          // cookie.
          /*expiration_time=*/base::Time::Now() + base::Days(730),
          /*last_access_time=*/base::Time(),
          /*secure=*/true,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_DEFAULT,
          /*partition_key=*/std::nullopt,
          /*status=*/nullptr);
  net::CookieOptions options;
  options.set_include_httponly();
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());

  ++active_cookie_manager_requests_for_testing_;

  network::mojom::CookieManager* cookie_manager = cookie_manager_cb_.Run();
  cookie_manager->SetCanonicalCookie(
      *cookie, url, options,
      base::BindOnce(
          &AccountConsistencyService::OnChromeConnectedCookieFinished,
          base::Unretained(this)));
}

void AccountConsistencyService::OnChromeConnectedCookieFinished(
    net::CookieAccessResult cookie_access_result) {
  DCHECK(cookie_access_result.status.IsInclude());
  --active_cookie_manager_requests_for_testing_;
}

void AccountConsistencyService::AddChromeConnectedCookies() {
  // These cookie requests are preventive. Chrome cannot be sure that
  // CHROME_CONNECTED cookies are set on google.com and youtube.com domains due
  // to ITP restrictions.
  SetChromeConnectedCookieWithUrls({GURL(kGoogleUrl), GURL(kYoutubeUrl)});
}

void AccountConsistencyService::OnBrowsingDataRemoved() {
  // SAPISID cookie has been removed, notify the GCMS.
  // TODO(crbug.com/40613324) : Remove the need to expose this method
  // or move it to the network::CookieManager.
  identity_manager_->GetAccountsCookieMutator()->ForceTriggerOnCookieChange();
}

void AccountConsistencyService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      AddChromeConnectedCookies();
      break;
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      RemoveAllChromeConnectedCookies(base::OnceClosure());
      break;
  }
}

void AccountConsistencyService::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  AddChromeConnectedCookies();

  // If signed-in accounts have been recently restored through GAIA cookie
  // restoration then run the relevant callback to finish the update process.
  if (accounts_in_cookie_jar_info.GetPotentiallyInvalidSignedInAccounts()
          .size() > 0) {
    RunGaiaCookiesRestoredCallbacks(/*has_cookie_changed=*/YES);
  }
}
