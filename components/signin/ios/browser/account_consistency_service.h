// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_IOS_BROWSER_ACCOUNT_CONSISTENCY_SERVICE_H_
#define COMPONENTS_SIGNIN_IOS_BROWSER_ACCOUNT_CONSISTENCY_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/ios/browser/active_state_manager.h"
#import "components/signin/ios/browser/manage_accounts_delegate.h"
#import "components/signin/public/identity_manager/identity_manager.h"

namespace content_settings {
class CookieSettings;
}

namespace web {
class BrowserState;
class WebState;
class WebStatePolicyDecider;
}

class AccountReconcilor;
class PrefService;

@class AccountConsistencyNavigationDelegate;
@class WKWebView;

// Handles actions necessary for Account Consistency to work on iOS. This
// includes setting the Account Consistency cookie (informing Gaia that the
// Account Consistency is on).
//
// This is currently only used when WKWebView is enabled.
class AccountConsistencyService : public KeyedService,
                                  public signin::IdentityManager::Observer,
                                  public ActiveStateManager::Observer {
 public:
  // Name of the preference property that persists the domains that have a
  // CHROME_CONNECTED cookie set by this service.
  static const char kDomainsWithCookiePref[];

  AccountConsistencyService(
      web::BrowserState* browser_state,
      PrefService* prefs,
      AccountReconcilor* account_reconcilor,
      scoped_refptr<content_settings::CookieSettings> cookie_settings,
      signin::IdentityManager* identity_manager);
  ~AccountConsistencyService() override;

  // Registers the preferences used by AccountConsistencyService.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Sets the handler for |web_state| that reacts on Gaia responses with the
  // X-Chrome-Manage-Accounts header and notifies |delegate|.
  void SetWebStateHandler(web::WebState* web_state,
                          id<ManageAccountsDelegate> delegate);
  // Removes the handler associated with |web_state|.
  void RemoveWebStateHandler(web::WebState* web_state);

  // Removes CHROME_CONNECTED cookies on all the Google domains where it was
  // set. Calls callback once all cookies were removed.
  void RemoveChromeConnectedCookies(base::OnceClosure callback);

  // Enqueues a request to add the CHROME_CONNECTED cookie to |domain|. If the
  // cookie is already on |domain|, this function will do nothing unless
  // |force_update_if_too_old| is true. In this case, the cookie will be
  // refreshed if it is considered too old.
  void AddChromeConnectedCookieToDomain(const std::string& domain,
                                        bool force_update_if_too_old);

  // Enqueues a request to remove the CHROME_CONNECTED cookie to |domain|.
  // Does nothing if the cookie is not set on |domain|.
  void RemoveChromeConnectedCookieFromDomain(const std::string& domain,
                                             base::OnceClosure callback);

  // Notifies the AccountConsistencyService that browsing data has been removed
  // for any time period.
  void OnBrowsingDataRemoved();

 private:
  friend class AccountConsistencyServiceTest;

  // The type of a cookie request.
  enum CookieRequestType {
    ADD_CHROME_CONNECTED_COOKIE,
    REMOVE_CHROME_CONNECTED_COOKIE
  };

  // A CHROME_CONNECTED cookie request to be applied by the
  // AccountConsistencyService.
  struct CookieRequest {
    static CookieRequest CreateAddCookieRequest(const std::string& domain);
    static CookieRequest CreateRemoveCookieRequest(const std::string& domain,
                                                   base::OnceClosure callback);
    CookieRequest();
    ~CookieRequest();

    // Movable, not copyable.
    CookieRequest(CookieRequest&&);
    CookieRequest& operator=(CookieRequest&&) = default;
    CookieRequest(const CookieRequest&) = delete;
    CookieRequest& operator=(const CookieRequest&) = delete;

    CookieRequestType request_type;
    std::string domain;
    base::OnceClosure callback;
  };

  // Loads the domains with a CHROME_CONNECTED cookie from the prefs.
  void LoadFromPrefs();

  // KeyedService implementation.
  void Shutdown() override;

  // Applies the pending CHROME_CONNECTED cookie requests one by one.
  void ApplyCookieRequests();

  // Called when the current CHROME_CONNECTED cookie request is done.
  void FinishedApplyingCookieRequest(bool success);

  // Returns the cached WKWebView if it exists, or creates one if necessary.
  // Can return nil if the browser state is not active.
  WKWebView* GetWKWebView();
  // Actually creates a WKWebView. Virtual for testing.
  virtual WKWebView* BuildWKWebView();
  // Stops any page loading in the WKWebView currently in use and releases it.
  void ResetWKWebView();

  // Returns whether the CHROME_CONNECTED cookie should be added to |domain|.
  // If the cookie is already on |domain|, this function will return false
  // unless |force_update_if_too_old| is true. In this case, it will return true
  // if the cookie is considered to be too old.
  bool ShouldAddChromeConnectedCookieToDomain(const std::string& domain,
                                              bool force_update_if_too_old);

  // Adds CHROME_CONNECTED cookies on all the main Google domains.
  void AddChromeConnectedCookies();

  // IdentityManager::Observer implementation.
  void OnPrimaryAccountSet(const CoreAccountInfo& account_info) override;
  void OnPrimaryAccountCleared(
      const CoreAccountInfo& previous_account_info) override;
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;

  // ActiveStateManager::Observer implementation.
  void OnActive() override;
  void OnInactive() override;

  // Browser state associated with the service, used to create WKWebViews.
  web::BrowserState* browser_state_;
  // Used to update kDomainsWithCookiePref.
  PrefService* prefs_;
  // Service managing accounts reconciliation, notified of GAIA responses with
  // the X-Chrome-Manage-Accounts header
  AccountReconcilor* account_reconcilor_;
  // Cookie settings currently in use for |browser_state_|, used to check if
  // setting CHROME_CONNECTED cookies is valid.
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  // Identity manager, observed to be notified of primary account signin and
  // signout events.
  signin::IdentityManager* identity_manager_;

  // Whether a CHROME_CONNECTED cookie request is currently being applied.
  bool applying_cookie_requests_;
  // The queue of CHROME_CONNECTED cookie requests to be applied.
  base::circular_deque<CookieRequest> cookie_requests_;
  // The map between domains where a CHROME_CONNECTED cookie is present and
  // the time when the cookie was last updated.
  std::map<std::string, base::Time> last_cookie_update_map_;

  // Web view used to apply the CHROME_CONNECTED cookie requests.
  __strong WKWebView* web_view_;
  // Navigation delegate of |web_view_| that informs the service when a cookie
  // request has been applied.
  AccountConsistencyNavigationDelegate* navigation_delegate_;

  // Handlers reacting on GAIA responses with the X-Chrome-Manage-Accounts
  // header set.
  std::map<web::WebState*, std::unique_ptr<web::WebStatePolicyDecider>>
      web_state_handlers_;

  DISALLOW_COPY_AND_ASSIGN(AccountConsistencyService);
};

#endif  // COMPONENTS_SIGNIN_IOS_BROWSER_ACCOUNT_CONSISTENCY_SERVICE_H_
