// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_IOS_BROWSER_ACCOUNT_CONSISTENCY_SERVICE_H_
#define COMPONENTS_SIGNIN_IOS_BROWSER_ACCOUNT_CONSISTENCY_SERVICE_H_

#include <map>
#include <set>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#import "components/signin/ios/browser/manage_accounts_delegate.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"

namespace content_settings {
class CookieSettings;
}

namespace web {
class WebState;
}

namespace network::mojom {
class CookieManager;
}

class AccountReconcilor;

// Handles actions necessary for keeping the list of Google accounts available
// on the web and those available on the iOS device from first-party Google apps
// consistent. This includes setting the Account Consistency cookie,
// CHROME_CONNECTED, which informs Gaia that the user is signed in to Chrome
// with Account Consistency on.
class AccountConsistencyService : public KeyedService,
                                  public signin::IdentityManager::Observer {
 public:
  // Callback used to get a CookieManager.
  using CookieManagerCallback =
      base::RepeatingCallback<network::mojom::CookieManager*()>;

  AccountConsistencyService(
      CookieManagerCallback cookie_manager_cb,
      AccountReconcilor* account_reconcilor,
      scoped_refptr<content_settings::CookieSettings> cookie_settings,
      signin::IdentityManager* identity_manager);

  AccountConsistencyService(const AccountConsistencyService&) = delete;
  AccountConsistencyService& operator=(const AccountConsistencyService&) =
      delete;

  ~AccountConsistencyService() override;

  // Sets the handler for |web_state| that reacts on Gaia responses with the
  // X-Chrome-Manage-Accounts header and notifies |delegate|.
  void SetWebStateHandler(web::WebState* web_state,
                          ManageAccountsDelegate* delegate);
  // Removes the handler associated with |web_state|.
  void RemoveWebStateHandler(web::WebState* web_state);

  // Notifies the AccountReconcilor that Gaia cookies have been deleted. Calls
  // callback once the Gaia cookies have been restored and returns YES on
  // success. Note that in order to avoid redirect loops this method applies a
  // one hour time restriction in between restoration calls.
  BOOL RestoreGaiaCookies(
      base::OnceCallback<void(BOOL)> cookies_restored_callback);

  // Enqueues a request to set the CHROME_CONNECTED cookie for the domain of the
  // |url|. The cookie is set if it is not already on the domain.
  void SetChromeConnectedCookieWithUrls(const std::vector<GURL>& urls);

  // Removes CHROME_CONNECTED cookies on all the Google domains where it was
  // set. Calls callback once all cookies were removed.
  void RemoveAllChromeConnectedCookies(base::OnceClosure callback);

  // Adds CHROME_CONNECTED cookies on all the main Google domains.
  void AddChromeConnectedCookies();

  // Notifies the AccountConsistencyService that browsing data has been removed
  // for any time period.
  void OnBrowsingDataRemoved();

 private:
  class AccountConsistencyHandler;
  friend class AccountConsistencyServiceTest;

  // KeyedService implementation.
  void Shutdown() override;

  // Sets the pending CHROME_CONNECTED cookie for the given |url|.
  void SetChromeConnectedCookieWithUrl(const GURL& url);

  // Called when the request to set CHROME_CONNECTED cookie is done.
  void OnChromeConnectedCookieFinished(
      net::CookieAccessResult cookie_access_result);

  // Called when cookie deletion is completed with the number of cookies that
  // were removed from the cookie store.
  void OnDeleteCookiesFinished(base::OnceClosure callback,
                               uint32_t num_cookies_deleted);

  // Triggers a Gaia cookie update on the Google domain. Calls
  // |cookies_restored_callback| with whether the Gaia cookies were restored.
  void TriggerGaiaCookieChangeIfDeleted(
      base::OnceCallback<void(BOOL)> cookies_restored_callback,
      const net::CookieAccessResultList& cookie_list,
      const net::CookieAccessResultList& excluded_cookies);

  // Runs the list of callbacks with |has_cookie_changed| to indicate whether
  // the cookies required a restore call.
  void RunGaiaCookiesRestoredCallbacks(BOOL has_cookie_changed);

  // IdentityManager::Observer implementation.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;

  // Callback used to get CookieManager.
  CookieManagerCallback cookie_manager_cb_;
  // Service managing accounts reconciliation, notified of GAIA responses with
  // the X-Chrome-Manage-Accounts header
  raw_ptr<AccountReconcilor> account_reconcilor_;
  // Cookie settings currently in use for |browser_state_|, used to check if
  // setting CHROME_CONNECTED cookies is valid.
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  // Identity manager, observed to be notified of primary account signin and
  // signout events.
  raw_ptr<signin::IdentityManager> identity_manager_;

  // The number of cookie manager requests that are being processed.
  // Used for testing purposes only.
  int64_t active_cookie_manager_requests_for_testing_;

  // Last time Gaia cookie was updated for the Google domain.
  base::Time last_gaia_cookie_update_time_;

  // List of callbacks to be called following GAIA cookie restoration.
  std::vector<base::OnceCallback<void(BOOL)>> gaia_cookies_restored_callbacks_;

  // Handlers reacting on GAIA responses with the X-Chrome-Manage-Accounts
  // header set.
  std::map<web::WebState*, std::unique_ptr<AccountConsistencyHandler>>
      handlers_map_;

  // Record whether Shutdown has been called.
  bool is_shutdown_ = false;
};

#endif  // COMPONENTS_SIGNIN_IOS_BROWSER_ACCOUNT_CONSISTENCY_SERVICE_H_
