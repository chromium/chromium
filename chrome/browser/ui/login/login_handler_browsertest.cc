// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <list>
#include <map>

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_origin.h"
#include "chrome/browser/ssl/ssl_blocking_page.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "chrome/browser/ui/login/login_handler_test_utils.h"
#include "chrome/browser/ui/login/login_interstitial_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/auth.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "url/gurl.h"

using content::NavigationController;
using content::OpenURLParams;
using content::Referrer;

namespace {

class LoginPromptBrowserTest : public InProcessBrowserTest {
 public:
  LoginPromptBrowserTest()
      : bad_password_("incorrect"),
        bad_username_("nouser"),
        password_("secret"),
        username_basic_("basicuser"),
        username_digest_("digestuser") {
    auth_map_["foo"] = AuthInfo("testuser", "foopassword");
    auth_map_["bar"] = AuthInfo("testuser", "barpassword");
    auth_map_["testrealm"] = AuthInfo(username_basic_, password_);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 protected:
  struct AuthInfo {
    std::string username_;
    std::string password_;

    AuthInfo() {}

    AuthInfo(const std::string& username,
             const std::string& password)
        : username_(username), password_(password) {}
  };

  typedef std::map<std::string, AuthInfo> AuthMap;

  void SetAuthFor(LoginHandler* handler);

  // Navigates to |visit_url| which triggers an HTTP auth dialog, and checks if
  // the URL displayed in the omnibox is equal to |expected_url| after all
  // navigations including page redirects are completed.
  // If |cancel_prompt| is true, the auth dialog is cancelled at the end.
  void TestCrossOriginPrompt(const GURL& visit_url,
                             const std::string& expected_hostname,
                             bool cancel_prompt) const;

  AuthMap auth_map_;
  std::string bad_password_;
  std::string bad_username_;
  std::string password_;
  std::string username_basic_;
  std::string username_digest_;
};

void LoginPromptBrowserTest::SetAuthFor(LoginHandler* handler) {
  const net::AuthChallengeInfo* challenge = handler->auth_info();

  ASSERT_TRUE(challenge);
  auto i = auth_map_.find(challenge->realm);
  EXPECT_TRUE(auth_map_.end() != i);
  if (i != auth_map_.end()) {
    const AuthInfo& info = i->second;
    handler->SetAuth(base::UTF8ToUTF16(info.username_),
                     base::UTF8ToUTF16(info.password_));
  }
}

const char kPrefetchAuthPage[] = "/login/prefetch.html";

const char kMultiRealmTestPage[] = "/login/multi_realm.html";
const int  kMultiRealmTestRealmCount = 2;
const int kMultiRealmTestAuthRequestsCount = 4;

const char kSingleRealmTestPage[] = "/login/single_realm.html";

const char kAuthBasicPage[] = "/auth-basic";
const char kAuthDigestPage[] = "/auth-digest";

// It does not matter what pages are selected as no-auth, as long as they exist.
// Navigating to non-existing pages caused flakes in the past
// (https://crbug.com/636875).
const char kNoAuthPage1[] = "/simple.html";
const char kNoAuthPage2[] = "/form.html";

base::string16 ExpectedTitleFromAuth(const base::string16& username,
                                     const base::string16& password) {
  // The TestServer sets the title to username/password on successful login.
  return username + base::UTF8ToUTF16("/") + password;
}

// Confirm that <link rel="prefetch"> targetting an auth required
// resource does not provide a login dialog.  These types of requests
// should instead just cancel the auth.

// Unfortunately, this test doesn't assert on anything for its
// correctness.  Instead, it relies on the auth dialog blocking the
// browser, and triggering a timeout to cause failure when the
// prefetch resource requires authorization.
IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest, PrefetchAuthCancels) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_page = embedded_test_server()->GetURL(kPrefetchAuthPage);

  class SetPrefetchForTest {
   public:
    explicit SetPrefetchForTest(bool prefetch)
        : old_prerender_mode_(prerender::PrerenderManager::GetMode()) {
      std::string exp_group = prefetch ? "ExperimentYes" : "ExperimentNo";
      base::FieldTrialList::CreateFieldTrial("Prefetch", exp_group);
      // Disable prerender so this is just a prefetch of the top-level page.
      prerender::PrerenderManager::SetMode(
          prerender::PrerenderManager::PRERENDER_MODE_SIMPLE_LOAD_EXPERIMENT);
    }

    ~SetPrefetchForTest() {
      prerender::PrerenderManager::SetMode(old_prerender_mode_);
    }

   private:
    prerender::PrerenderManager::PrerenderManagerMode old_prerender_mode_;
  } set_prefetch_for_test(true);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  NavigationController* controller = &contents->GetController();
  LoginPromptBrowserTestObserver observer;

  observer.Register(content::Source<NavigationController>(controller));

  WindowedLoadStopObserver load_stop_waiter(controller, 1);
  browser()->OpenURL(OpenURLParams(test_page, Referrer(),
                                   WindowOpenDisposition::CURRENT_TAB,
                                   ui::PAGE_TRANSITION_TYPED, false));

  load_stop_waiter.Wait();
  EXPECT_TRUE(observer.handlers().empty());
}

// Test that "Basic" HTTP authentication works.
IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest, TestBasicAuth) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_page = embedded_test_server()->GetURL(kAuthBasicPage);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  NavigationController* controller = &contents->GetController();

  // If the network service crashes, basic auth should still be enabled.
  for (bool crash_network_service : {false, true}) {
    if (crash_network_service) {
      // Can't crash the network service if it isn't enabled.
      if (!base::FeatureList::IsEnabled(network::features::kNetworkService))
        return;

      SimulateNetworkServiceCrash();
      // Flush the network interface to make sure it notices the crash.
      content::BrowserContext::GetDefaultStoragePartition(browser()->profile())
          ->FlushNetworkInterfaceForTesting();
    }

    LoginPromptBrowserTestObserver observer;

    observer.Register(content::Source<NavigationController>(controller));

    {
      WindowedAuthNeededObserver auth_needed_waiter(controller);
      browser()->OpenURL(OpenURLParams(test_page, Referrer(),
                                       WindowOpenDisposition::CURRENT_TAB,
                                       ui::PAGE_TRANSITION_TYPED, false));
      auth_needed_waiter.Wait();
    }

    ASSERT_FALSE(observer.handlers().empty());
    {
      WindowedAuthNeededObserver auth_needed_waiter(controller);
      WindowedAuthSuppliedObserver auth_supplied_waiter(controller);
      LoginHandler* handler = *observer.handlers().begin();

      ASSERT_TRUE(handler);
      handler->SetAuth(base::UTF8ToUTF16(bad_username_),
                       base::UTF8ToUTF16(bad_password_));
      auth_supplied_waiter.Wait();

      // The request should be retried after the incorrect password is
      // supplied.  This should result in a new AUTH_NEEDED notification
      // for the same realm.
      auth_needed_waiter.Wait();
    }

    ASSERT_EQ(1u, observer.handlers().size());
    WindowedAuthSuppliedObserver auth_supplied_waiter(controller);
    LoginHandler* handler = *observer.handlers().begin();
    SetAuthFor(handler);
    auth_supplied_waiter.Wait();

    base::string16 expected_title = ExpectedTitleFromAuth(
        base::ASCIIToUTF16("basicuser"), base::ASCIIToUTF16("secret"));
    content::TitleWatcher title_watcher(contents, expected_title);
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }
}

// Test that "Digest" HTTP authentication works.
IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest, TestDigestAuth) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_page = embedded_test_server()->GetURL(kAuthDigestPage);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  NavigationController* controller = &contents->GetController();
  LoginPromptBrowserTestObserver observer;

  observer.Register(content::Source<NavigationController>(controller));

  {
    WindowedAuthNeededObserver auth_needed_waiter(controller);
    browser()->OpenURL(OpenURLParams(test_page, Referrer(),
                                     WindowOpenDisposition::CURRENT_TAB,
                                     ui::PAGE_TRANSITION_TYPED, false));
    auth_needed_waiter.Wait();
  }

  ASSERT_FALSE(observer.handlers().empty());
  {
    WindowedAuthNeededObserver auth_needed_waiter(controller);
    WindowedAuthSuppliedObserver auth_supplied_waiter(controller);
    LoginHandler* handler = *observer.handlers().begin();

    ASSERT_TRUE(handler);
    handler->SetAuth(base::UTF8ToUTF16(bad_username_),
                     base::UTF8ToUTF16(bad_password_));
    auth_supplied_waiter.Wait();

    // The request should be retried after the incorrect password is
    // supplied.  This should result in a new AUTH_NEEDED notification
    // for the same realm.
    auth_needed_waiter.Wait();
  }

  ASSERT_EQ(1u, observer.handlers().size());
  WindowedAuthSuppliedObserver auth_supplied_waiter(controller);
  LoginHandler* handler = *observer.handlers().begin();

  base::string16 username(base::UTF8ToUTF16(username_digest_));
  base::string16 password(base::UTF8ToUTF16(password_));
  handler->SetAuth(username, password);
  auth_supplied_waiter.Wait();

  base::string16 expected_title = ExpectedTitleFromAuth(username, password);
  content::TitleWatcher title_watcher(contents, expected_title);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest, TestTwoAuths) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* contents1 =
      browser()->tab_strip_model()->GetActiveWebContents();
  NavigationController* controller1 = &contents1->GetController();
  LoginPromptBrowserTestObserver observer;

  observer.Register(content::Source<NavigationController>(controller1));

  // Open a new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  content::WebContents* contents2 =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(contents1, contents2);
  NavigationController* controller2 = &contents2->GetController();
  observer.Register(content::Source<NavigationController>(controller2));

  {
    WindowedAuthNeededObserver auth_needed_waiter(controller1);
    contents1->OpenURL(OpenURLParams(
        embedded_test_server()->GetURL(kAuthBasicPage), Referrer(),
        WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false));
    auth_needed_waiter.Wait();
  }

  {
    WindowedAuthNeededObserver auth_needed_waiter(controller2);
    contents2->OpenURL(OpenURLParams(
        embedded_test_server()->GetURL(kAuthDigestPage), Referrer(),
        WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false));
    auth_needed_waiter.Wait();
  }

  ASSERT_EQ(2u, observer.handlers().size());

  LoginHandler* handler1 = *observer.handlers().begin();
  LoginHandler* handler2 = *(++(observer.handlers().begin()));

  base::string16 expected_title1 = ExpectedTitleFromAuth(
      base::UTF8ToUTF16(username_basic_), base::UTF8ToUTF16(password_));
  base::string16 expected_title2 = ExpectedTitleFromAuth(
      base::UTF8ToUTF16(username_digest_), base::UTF8ToUTF16(password_));
  content::TitleWatcher title_watcher1(contents1, expected_title1);
  content::TitleWatcher title_watcher2(contents2, expected_title2);

  handler1->SetAuth(base::UTF8ToUTF16(username_basic_),
                    base::UTF8ToUTF16(password_));
  handler2->SetAuth(base::UTF8ToUTF16(username_digest_),
                    base::UTF8ToUTF16(password_));

  EXPECT_EQ(expected_title1, title_watcher1.WaitAndGetTitle());
  EXPECT_EQ(expected_title2, title_watcher2.WaitAndGetTitle());
}

// Test manual login prompt cancellation.
IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest, TestCancelAuth_Manual) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kAuthURL = embedded_test_server()->GetURL(kAuthBasicPage);

  NavigationController* controller =
      &browser()->tab_strip_model()->GetActiveWebContents()->GetController();

  LoginPromptBrowserTestObserver observer;
  observer.Register(content::Source<NavigationController>(controller));

  WindowedLoadStopObserver load_stop_waiter(controller, 1);
  WindowedAuthNeededObserver auth_needed_waiter(controller);
  browser()->OpenURL(OpenURLParams(kAuthURL, Referrer(),
                                   WindowOpenDisposition::CURRENT_TAB,
                                   ui::PAGE_TRANSITION_TYPED, false));
  auth_needed_waiter.Wait();
  WindowedAuthCancelledObserver auth_cancelled_waiter(controller);
  LoginHandler* handler = *observer.handlers().begin();
  ASSERT_TRUE(handler);
  handler->CancelAuth();
  auth_cancelled_waiter.Wait();
  load_stop_waiter.Wait();
  EXPECT_TRUE(observer.handlers().empty());
}

// Test login prompt cancellation on navigation to a new page.
IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest, TestCancelAuth_OnNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kAuthURL = embedded_test_server()->GetURL(kAuthBasicPage);
  const GURL kNoAuthURL = embedded_test_server()->GetURL(kNoAuthPage1);

  NavigationController* controller =
      &browser()->tab_strip_model()->GetActiveWebContents()->GetController();

  LoginPromptBrowserTestObserver observer;
  observer.Register(content::Source<NavigationController>(controller));

  // One LOAD_STOP event for kAuthURL and second  for kNoAuthURL.
  WindowedLoadStopObserver load_stop_waiter(controller, 2);
  WindowedAuthNeededObserver auth_needed_waiter(controller);
  browser()->OpenURL(OpenURLParams(kAuthURL, Referrer(),
                                   WindowOpenDisposition::CURRENT_TAB,
                                   ui::PAGE_TRANSITION_TYPED, false));
  auth_needed_waiter.Wait();
  WindowedAuthCancelledObserver auth_cancelled_waiter(controller);
  // Navigating while auth is requested is the same as cancelling.
  browser()->OpenURL(OpenURLParams(kNoAuthURL, Referrer(),
                                   WindowOpenDisposition::CURRENT_TAB,
                                   ui::PAGE_TRANSITION_TYPED, false));
  auth_cancelled_waiter.Wait();
  load_stop_waiter.Wait();
  EXPECT_TRUE(observer.handlers().empty());
}

// Test login prompt cancellation on navigation to back.
IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest, TestCancelAuth_OnBack) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kAuthURL = embedded_test_server()->GetURL(kAuthBasicPage);
  const GURL kNoAuthURL = embedded_test_server()->GetURL(kNoAuthPage1);

  NavigationController* controller =
      &browser()->tab_strip_model()->GetActiveWebContents()->GetController();

  LoginPromptBrowserTestObserver observer;
  observer.Register(content::Source<NavigationController>(controller));

  // First navigate to an unauthenticated page so we have something to
  // go back to.
  ui_test_utils::NavigateToURL(browser(), kNoAuthURL);

  WindowedLoadStopObserver load_stop_waiter(controller, 1);
  WindowedAuthNeededObserver auth_needed_waiter(controller);
  browser()->OpenURL(OpenURLParams(kAuthURL, Referrer(),
                                   WindowOpenDisposition::CURRENT_TAB,
                                   ui::PAGE_TRANSITION_TYPED, false));
  auth_needed_waiter.Wait();
  WindowedAuthCancelledObserver auth_cancelled_waiter(controller);
  // Navigating back while auth is requested is the same as cancelling.
  ASSERT_TRUE(controller->CanGoBack());
  controller->GoBack();
  auth_cancelled_waiter.Wait();
  load_stop_waiter.Wait();
  EXPECT_TRUE(observer.handlers().empty());
}

// Test login prompt cancellation on navigation to forward.
IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest, TestCancelAuth_OnForward) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kAuthURL = embedded_test_server()->GetURL(kAuthBasicPage);
  const GURL kNoAuthURL1 = embedded_test_server()->GetURL(kNoAuthPage1);
  const GURL kNoAuthURL2 = embedded_test_server()->GetURL(kNoAuthPage2);

  NavigationController* controller =
      &browser()->tab_strip_model()->GetActiveWebContents()->GetController();

  LoginPromptBrowserTestObserver observer;
  observer.Register(content::Source<NavigationController>(controller));

  ui_test_utils::NavigateToURL(browser(), kNoAuthURL1);

  // Now add a page and go back, so we have something to go forward to.
  ui_test_utils::NavigateToURL(browser(), kNoAuthURL2);
  {
    WindowedLoadStopObserver load_stop_waiter(controller, 1);
    ASSERT_TRUE(controller->CanGoBack());
    controller->GoBack();
    load_stop_waiter.Wait();
  }

  WindowedLoadStopObserver load_stop_waiter(controller, 1);
  WindowedAuthNeededObserver auth_needed_waiter(controller);
  browser()->OpenURL(OpenURLParams(kAuthURL, Referrer(),
                                   WindowOpenDisposition::CURRENT_TAB,
                                   ui::PAGE_TRANSITION_TYPED, false));
  auth_needed_waiter.Wait();
  WindowedAuthCancelledObserver auth_cancelled_waiter(controller);
  ASSERT_TRUE(controller->CanGoForward());
  controller->GoForward();
  auth_cancelled_waiter.Wait();
  load_stop_waiter.Wait();
  EXPECT_TRUE(observer.handlers().empty());
}

// Test handling of resources that require authentication even though
// the page they are included on doesn't.  In this case we should only
// present the minimal number of prompts necessary for successfully
// displaying the page.
class MultiRealmLoginPromptBrowserTest : public LoginPromptBrowserTest {
 public:
  void TearDownOnMainThread() override {
    login_prompt_observer_.UnregisterAll();
    LoginPromptBrowserTest::TearDownOnMainThread();
  }

  // Load the multi-realm test page, waits for LoginHandlers to be created, then
  // calls |for_each_realm_func| once for each authentication realm, passing a
  // LoginHandler for the realm as an argument. The page should stop loading
  // after that.
  template <class F>
  void RunTest(const F& for_each_realm_func);

  NavigationController* GetNavigationController() {
    return &browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetController();
  }

  LoginPromptBrowserTestObserver* login_prompt_observer() {
    return &login_prompt_observer_;
  }

 private:
  LoginPromptBrowserTestObserver login_prompt_observer_;
};

template <class F>
void MultiRealmLoginPromptBrowserTest::RunTest(const F& for_each_realm_func) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_page = embedded_test_server()->GetURL(kMultiRealmTestPage);

  NavigationController* controller = GetNavigationController();

  login_prompt_observer_.Register(
      content::Source<NavigationController>(controller));

  WindowedLoadStopObserver load_stop_waiter(controller, 1);

  browser()->OpenURL(OpenURLParams(test_page, Referrer(),
                                   WindowOpenDisposition::CURRENT_TAB,
                                   ui::PAGE_TRANSITION_TYPED, false));

  // Need to have LoginHandlers created for all requests that need
  // authentication.
  while (login_prompt_observer_.handlers().size() <
         kMultiRealmTestAuthRequestsCount)
    WindowedAuthNeededObserver(controller).Wait();

  // Now confirm or cancel auth once per realm.
  std::set<std::string> seen_realms;
  for (int i = 0; i < kMultiRealmTestRealmCount; ++i) {
    auto it = std::find_if(
        login_prompt_observer_.handlers().begin(),
        login_prompt_observer_.handlers().end(),
        [&seen_realms](LoginHandler* handler) {
          return seen_realms.count(handler->auth_info()->realm) == 0;
        });
    ASSERT_TRUE(it != login_prompt_observer_.handlers().end());
    seen_realms.insert((*it)->auth_info()->realm);

    for_each_realm_func(*it);
  }

  load_stop_waiter.Wait();
}

// Checks that cancelling works as expected.
IN_PROC_BROWSER_TEST_F(MultiRealmLoginPromptBrowserTest,
                       MultipleRealmCancellation) {
  RunTest([this](LoginHandler* handler) {
    WindowedAuthCancelledObserver waiter(GetNavigationController());
    handler->CancelAuth();
    waiter.Wait();
  });

  EXPECT_EQ(0, login_prompt_observer()->auth_supplied_count());
  EXPECT_LT(0, login_prompt_observer()->auth_needed_count());
  EXPECT_LT(0, login_prompt_observer()->auth_cancelled_count());
}

// Checks that supplying credentials works as expected.
IN_PROC_BROWSER_TEST_F(MultiRealmLoginPromptBrowserTest,
                       MultipleRealmConfirmation) {
  RunTest([this](LoginHandler* handler) {
    WindowedAuthSuppliedObserver waiter(GetNavigationController());
    SetAuthFor(handler);
    waiter.Wait();
  });

  EXPECT_LT(0, login_prompt_observer()->auth_needed_count());
  EXPECT_LT(0, login_prompt_observer()->auth_supplied_count());
  EXPECT_EQ(0, login_prompt_observer()->auth_cancelled_count());
}

// Testing for recovery from an incorrect password for the case where
// there are multiple authenticated resources.
IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest, IncorrectConfirmation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_page = embedded_test_server()->GetURL(kSingleRealmTestPage);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  NavigationController* controller = &contents->GetController();
  LoginPromptBrowserTestObserver observer;

  observer.Register(content::Source<NavigationController>(controller));

  {
    WindowedAuthNeededObserver auth_needed_waiter(controller);
    browser()->OpenURL(OpenURLParams(test_page, Referrer(),
                                     WindowOpenDisposition::CURRENT_TAB,
                                     ui::PAGE_TRANSITION_TYPED, false));
    auth_needed_waiter.Wait();
  }

  EXPECT_FALSE(observer.handlers().empty());

  if (!observer.handlers().empty()) {
    WindowedAuthNeededObserver auth_needed_waiter(controller);
    WindowedAuthSuppliedObserver auth_supplied_waiter(controller);
    LoginHandler* handler = *observer.handlers().begin();

    ASSERT_TRUE(handler);
    handler->SetAuth(base::UTF8ToUTF16(bad_username_),
                     base::UTF8ToUTF16(bad_password_));
    auth_supplied_waiter.Wait();

    // The request should be retried after the incorrect password is
    // supplied.  This should result in a new AUTH_NEEDED notification
    // for the same realm.
    auth_needed_waiter.Wait();
  }

  int n_handlers = 0;

  while (n_handlers < 1) {
    WindowedAuthNeededObserver auth_needed_waiter(controller);

    while (!observer.handlers().empty()) {
      WindowedAuthSuppliedObserver auth_supplied_waiter(controller);
      LoginHandler* handler = *observer.handlers().begin();

      ASSERT_TRUE(handler);
      n_handlers++;
      SetAuthFor(handler);
      auth_supplied_waiter.Wait();
    }

    if (n_handlers < 1)
      auth_needed_waiter.Wait();
  }

  // The single realm test has only one realm, and thus only one login
  // prompt.
  EXPECT_EQ(1, n_handlers);
  EXPECT_LT(0, observer.auth_needed_count());
  EXPECT_EQ(0, observer.auth_cancelled_count());
  EXPECT_EQ(observer.auth_needed_count(), observer.auth_supplied_count());
}

// If the favicon is an authenticated resource, we shouldn't prompt
// for credentials.  The same URL, if requested elsewhere should
// prompt for credentials.
IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest, NoLoginPromptForFavicon) {
  const char kFaviconTestPage[] = "/login/has_favicon.html";
  const char kFaviconResource[] = "/auth-basic/favicon.gif";

  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  NavigationController* controller = &contents->GetController();
  LoginPromptBrowserTestObserver observer;

  observer.Register(content::Source<NavigationController>(controller));

  // First load a page that has a favicon that requires
  // authentication.  There should be no login prompt.
  {
    GURL test_page = embedded_test_server()->GetURL(kFaviconTestPage);
    WindowedLoadStopObserver load_stop_waiter(controller, 1);
    browser()->OpenURL(OpenURLParams(test_page, Referrer(),
                                     WindowOpenDisposition::CURRENT_TAB,
                                     ui::PAGE_TRANSITION_TYPED, false));
    load_stop_waiter.Wait();
  }

  // Now request the same favicon, but directly as the document.
  // There should be one login prompt.
  {
    GURL test_page = embedded_test_server()->GetURL(kFaviconResource);
    WindowedLoadStopObserver load_stop_waiter(controller, 1);
    WindowedAuthNeededObserver auth_needed_waiter(controller);
    browser()->OpenURL(OpenURLParams(test_page, Referrer(),
                                     WindowOpenDisposition::CURRENT_TAB,
                                     ui::PAGE_TRANSITION_TYPED, false));
    auth_needed_waiter.Wait();
    ASSERT_EQ(1u, observer.handlers().size());

    while (!observer.handlers().empty()) {
      WindowedAuthCancelledObserver auth_cancelled_waiter(controller);
      LoginHandler* handler = *observer.handlers().begin();

      ASSERT_TRUE(handler);
      handler->CancelAuth();
      auth_cancelled_waiter.Wait();
    }

    load_stop_waiter.Wait();
  }

  EXPECT_EQ(0, observer.auth_supplied_count());
  EXPECT_EQ(1, observer.auth_needed_count());
  EXPECT_EQ(1, observer.auth_cancelled_count());
}

// Block crossdomain image login prompting as a phishing defense.
IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest,
                       BlockCrossdomainPromptForSubresources) {
  const char kTestPage[] = "/login/load_img_from_b.html";

  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  NavigationController* controller = &contents->GetController();
  LoginPromptBrowserTestObserver observer;
  observer.Register(content::Source<NavigationController>(controller));

  // Load a page that has a cross-domain sub-resource authentication.
  // There should be no login prompt.
  {
    GURL test_page = embedded_test_server()->GetURL(kTestPage);
    ASSERT_EQ("127.0.0.1", test_page.host());

    // Change the host from 127.0.0.1 to www.a.com so that when the
    // page tries to load from b, it will be cross-origin.
    GURL::Replacements replacements;
    replacements.SetHostStr("www.a.com");
    test_page = test_page.ReplaceComponents(replacements);

    WindowedLoadStopObserver load_stop_waiter(controller, 1);
    browser()->OpenURL(OpenURLParams(test_page, Referrer(),
                                     WindowOpenDisposition::CURRENT_TAB,
                                     ui::PAGE_TRANSITION_TYPED, false));
    load_stop_waiter.Wait();
  }

  EXPECT_EQ(0, observer.auth_needed_count());

  // Now request the same page, but from the same origin.
  // There should be one login prompt.
  {
    GURL test_page = embedded_test_server()->GetURL(kTestPage);
    ASSERT_EQ("127.0.0.1", test_page.host());

    // Change the host from 127.0.0.1 to www.b.com so that when the
    // page tries to load from b, it will be same-origin.
    GURL::Replacements replacements;
    replacements.SetHostStr("www.b.com");
    test_page = test_page.ReplaceComponents(replacements);

    WindowedAuthNeededObserver auth_needed_waiter(controller);
    browser()->OpenURL(OpenURLParams(test_page, Referrer(),
                                     WindowOpenDisposition::CURRENT_TAB,
                                     ui::PAGE_TRANSITION_TYPED, false));
    auth_needed_waiter.Wait();
    ASSERT_EQ(1u, observer.handlers().size());

    while (!observer.handlers().empty()) {
      WindowedAuthCancelledObserver auth_cancelled_waiter(controller);
      LoginHandler* handler = *observer.handlers().begin();

      ASSERT_TRUE(handler);
      handler->CancelAuth();
      auth_cancelled_waiter.Wait();
    }
  }

  EXPECT_EQ(1, observer.auth_needed_count());
}

// Block same domain image resource if the top level frame is HTTPS and the
// image resource is HTTP.
// E.g. Top level: https://example.com, Image resource: http://example.com/image
IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest,
                       BlockCrossdomainPromptForSubresourcesMixedContent) {
  ASSERT_TRUE(embedded_test_server()->Start());
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server.Start());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  NavigationController* controller = &contents->GetController();
  LoginPromptBrowserTestObserver observer;
  observer.Register(content::Source<NavigationController>(controller));

  GURL image_url = embedded_test_server()->GetURL("/auth-basic/index.html");
  GURL test_page = https_server.GetURL(
      std::string("/login/load_img_from_same_domain_mixed_content.html?") +
      image_url.spec());
  GURL::Replacements replacements;
  replacements.SetHostStr("a.com");
  test_page = test_page.ReplaceComponents(replacements);
  image_url = image_url.ReplaceComponents(replacements);

  WindowedLoadStopObserver load_stop_waiter(controller, 1);
  browser()->OpenURL(OpenURLParams(test_page, Referrer(),
                                   WindowOpenDisposition::CURRENT_TAB,
                                   ui::PAGE_TRANSITION_TYPED, false));
  load_stop_waiter.Wait();
  EXPECT_EQ(0, observer.auth_needed_count());
}

// Allow crossdomain iframe login prompting despite the above.
IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest,
                       AllowCrossdomainPromptForSubframes) {
  const char kTestPage[] = "/login/load_iframe_from_b.html";

  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  NavigationController* controller = &contents->GetController();
  LoginPromptBrowserTestObserver observer;
  observer.Register(content::Source<NavigationController>(controller));

  // Load a page that has a cross-domain iframe authentication.
  {
    GURL test_page = embedded_test_server()->GetURL(kTestPage);
    ASSERT_EQ("127.0.0.1", test_page.host());

    // Change the host from 127.0.0.1 to www.a.com so that when the
    // page tries to load from b, it will be cross-origin.
    static const char kNewHost[] = "www.a.com";
    GURL::Replacements replacements;
    replacements.SetHostStr(kNewHost);
    test_page = test_page.ReplaceComponents(replacements);

    WindowedAuthNeededObserver auth_needed_waiter(controller);
    browser()->OpenURL(OpenURLParams(test_page, Referrer(),
                                     WindowOpenDisposition::CURRENT_TAB,
                                     ui::PAGE_TRANSITION_TYPED, false));
    auth_needed_waiter.Wait();
    ASSERT_EQ(1u, observer.handlers().size());

    while (!observer.handlers().empty()) {
      WindowedAuthCancelledObserver auth_cancelled_waiter(controller);
      LoginHandler* handler = *observer.handlers().begin();

      ASSERT_TRUE(handler);
      // When a cross origin iframe displays a login prompt, the blank
      // interstitial shouldn't be displayed and the omnibox should show the
      // main frame's url, not the iframe's.
      EXPECT_EQ(kNewHost, contents->GetVisibleURL().host());

      handler->CancelAuth();
      auth_cancelled_waiter.Wait();
    }
  }

  // Should stay on the main frame's url once the prompt the iframe is closed.
  EXPECT_EQ("www.a.com", contents->GetVisibleURL().host());

  EXPECT_EQ(1, observer.auth_needed_count());
}

IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest, SupplyRedundantAuths) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Get NavigationController for tab 1.
  content::WebContents* contents_1 =
      browser()->tab_strip_model()->GetActiveWebContents();
  NavigationController* controller_1 = &contents_1->GetController();

  // Open a new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  // Get NavigationController for tab 2.
  content::WebContents* contents_2 =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(contents_1, contents_2);
  NavigationController* controller_2 = &contents_2->GetController();

  LoginPromptBrowserTestObserver observer;
  observer.Register(content::Source<NavigationController>(controller_1));
  observer.Register(content::Source<NavigationController>(controller_2));

  {
    // Open different auth urls in each tab.
    WindowedAuthNeededObserver auth_needed_waiter_1(controller_1);
    contents_1->OpenURL(OpenURLParams(
        embedded_test_server()->GetURL("/auth-basic/1"), content::Referrer(),
        WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false));
    auth_needed_waiter_1.Wait();

    WindowedAuthNeededObserver auth_needed_waiter_2(controller_2);
    contents_2->OpenURL(OpenURLParams(
        embedded_test_server()->GetURL("/auth-basic/2"), content::Referrer(),
        WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false));
    auth_needed_waiter_2.Wait();

    ASSERT_EQ(2U, observer.handlers().size());

    // Supply auth in one of the tabs.
    WindowedAuthSuppliedObserver auth_supplied_waiter_1(controller_1);
    WindowedAuthSuppliedObserver auth_supplied_waiter_2(controller_2);
    LoginHandler* handler_1 = *observer.handlers().begin();
    ASSERT_TRUE(handler_1);
    SetAuthFor(handler_1);

    // Both tabs should be authenticated.
    auth_supplied_waiter_1.Wait();
    auth_supplied_waiter_2.Wait();
  }

  EXPECT_EQ(2, observer.auth_needed_count());
  EXPECT_EQ(2, observer.auth_supplied_count());
  EXPECT_EQ(0, observer.auth_cancelled_count());
}

IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest, CancelRedundantAuths) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Get NavigationController for tab 1.
  content::WebContents* contents_1 =
      browser()->tab_strip_model()->GetActiveWebContents();
  NavigationController* controller_1 = &contents_1->GetController();

  // Open a new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  // Get NavigationController for tab 2.
  content::WebContents* contents_2 =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(contents_1, contents_2);
  NavigationController* controller_2 = &contents_2->GetController();

  LoginPromptBrowserTestObserver observer;
  observer.Register(content::Source<NavigationController>(controller_1));
  observer.Register(content::Source<NavigationController>(controller_2));

  {
    // Open different auth urls in each tab.
    WindowedAuthNeededObserver auth_needed_waiter_1(controller_1);
    contents_1->OpenURL(OpenURLParams(
        embedded_test_server()->GetURL("/auth-basic/1"), content::Referrer(),
        WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false));
    auth_needed_waiter_1.Wait();

    WindowedAuthNeededObserver auth_needed_waiter_2(controller_2);
    contents_2->OpenURL(OpenURLParams(
        embedded_test_server()->GetURL("/auth-basic/2"), content::Referrer(),
        WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false));
    auth_needed_waiter_2.Wait();

    ASSERT_EQ(2U, observer.handlers().size());

    // Cancel auth in one of the tabs.
    WindowedAuthCancelledObserver auth_cancelled_waiter_1(controller_1);
    WindowedAuthCancelledObserver auth_cancelled_waiter_2(controller_2);
    LoginHandler* handler_1 = *observer.handlers().begin();
    ASSERT_TRUE(handler_1);
    handler_1->CancelAuth();

    // Both tabs should cancel auth.
    auth_cancelled_waiter_1.Wait();
    auth_cancelled_waiter_2.Wait();
  }

  EXPECT_EQ(2, observer.auth_needed_count());
  EXPECT_EQ(0, observer.auth_supplied_count());
  EXPECT_EQ(2, observer.auth_cancelled_count());
}

IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest,
                       SupplyRedundantAuthsMultiProfile) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Get NavigationController for regular tab.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  NavigationController* controller = &contents->GetController();

  // Open an incognito window.
  Browser* browser_incognito = CreateIncognitoBrowser();

  // Get NavigationController for incognito tab.
  content::WebContents* contents_incognito =
      browser_incognito->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(contents, contents_incognito);
  NavigationController* controller_incognito =
      &contents_incognito->GetController();

  LoginPromptBrowserTestObserver observer;
  observer.Register(content::Source<NavigationController>(controller));
  LoginPromptBrowserTestObserver observer_incognito;
  observer_incognito.Register(
      content::Source<NavigationController>(controller_incognito));

  {
    // Open an auth url in each window.
    WindowedAuthNeededObserver auth_needed_waiter(controller);
    WindowedAuthNeededObserver auth_needed_waiter_incognito(
        controller_incognito);
    contents->OpenURL(OpenURLParams(
        embedded_test_server()->GetURL("/auth-basic/1"), content::Referrer(),
        WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false));
    contents_incognito->OpenURL(OpenURLParams(
        embedded_test_server()->GetURL("/auth-basic/2"), content::Referrer(),
        WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false));
    auth_needed_waiter.Wait();
    auth_needed_waiter_incognito.Wait();

    ASSERT_EQ(1U, observer.handlers().size());
    ASSERT_EQ(1U, observer_incognito.handlers().size());

    // Supply auth in regular tab.
    WindowedAuthSuppliedObserver auth_supplied_waiter(controller);
    LoginHandler* handler = *observer.handlers().begin();
    ASSERT_TRUE(handler);
    SetAuthFor(handler);

    // Regular tab should be authenticated.
    auth_supplied_waiter.Wait();

    // There's not really a way to wait for the incognito window to "do
    // nothing".  Run anything pending in the message loop just to be sure.
    // (This shouldn't be necessary since notifications are synchronous, but
    // maybe it will help avoid flake someday in the future..)
    content::RunAllPendingInMessageLoop();
  }

  EXPECT_EQ(1, observer.auth_needed_count());
  EXPECT_EQ(1, observer.auth_supplied_count());
  EXPECT_EQ(0, observer.auth_cancelled_count());
  EXPECT_EQ(1, observer_incognito.auth_needed_count());
  EXPECT_EQ(0, observer_incognito.auth_supplied_count());
  EXPECT_EQ(0, observer_incognito.auth_cancelled_count());
}

// If an XMLHttpRequest is made with incorrect credentials, there should be no
// login prompt; instead the 401 status should be returned to the script.
IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest,
                       NoLoginPromptForXHRWithBadCredentials) {
  const char kXHRTestPage[] = "/login/xhr_with_credentials.html#incorrect";

  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  NavigationController* controller = &contents->GetController();
  LoginPromptBrowserTestObserver observer;

  observer.Register(content::Source<NavigationController>(controller));

  // Load a page which makes a synchronous XMLHttpRequest for an authenticated
  // resource with the wrong credentials.  There should be no login prompt.
  {
    GURL test_page = embedded_test_server()->GetURL(kXHRTestPage);
    WindowedLoadStopObserver load_stop_waiter(controller, 1);
    browser()->OpenURL(OpenURLParams(test_page, Referrer(),
                                     WindowOpenDisposition::CURRENT_TAB,
                                     ui::PAGE_TRANSITION_TYPED, false));
    load_stop_waiter.Wait();
  }

  base::string16 expected_title(base::UTF8ToUTF16("status=401"));

  EXPECT_EQ(expected_title, contents->GetTitle());
  EXPECT_EQ(0, observer.auth_supplied_count());
  EXPECT_EQ(0, observer.auth_needed_count());
  EXPECT_EQ(0, observer.auth_cancelled_count());
}

// If an XMLHttpRequest is made with correct credentials, there should be no
// login prompt either.
IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest,
                       NoLoginPromptForXHRWithGoodCredentials) {
  const char kXHRTestPage[] = "/login/xhr_with_credentials.html#secret";

  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  NavigationController* controller = &contents->GetController();
  LoginPromptBrowserTestObserver observer;

  observer.Register(content::Source<NavigationController>(controller));

  // Load a page which makes a synchronous XMLHttpRequest for an authenticated
  // resource with the wrong credentials.  There should be no login prompt.
  {
    GURL test_page = embedded_test_server()->GetURL(kXHRTestPage);
    WindowedLoadStopObserver load_stop_waiter(controller, 1);
    browser()->OpenURL(OpenURLParams(test_page, Referrer(),
                                     WindowOpenDisposition::CURRENT_TAB,
                                     ui::PAGE_TRANSITION_TYPED, false));
    load_stop_waiter.Wait();
  }

  base::string16 expected_title(base::UTF8ToUTF16("status=200"));

  EXPECT_EQ(expected_title, contents->GetTitle());
  EXPECT_EQ(0, observer.auth_supplied_count());
  EXPECT_EQ(0, observer.auth_needed_count());
  EXPECT_EQ(0, observer.auth_cancelled_count());
}

// If an XMLHttpRequest is made without credentials, there should be a login
// prompt.
IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest,
                       LoginPromptForXHRWithoutCredentials) {
  const char kXHRTestPage[] = "/login/xhr_without_credentials.html";

  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  NavigationController* controller = &contents->GetController();
  LoginPromptBrowserTestObserver observer;

  observer.Register(content::Source<NavigationController>(controller));

  // Load a page which makes a synchronous XMLHttpRequest for an authenticated
  // resource with the wrong credentials.  There should be no login prompt.
  {
    GURL test_page = embedded_test_server()->GetURL(kXHRTestPage);
    WindowedAuthNeededObserver auth_needed_waiter(controller);
    browser()->OpenURL(OpenURLParams(test_page, Referrer(),
                                     WindowOpenDisposition::CURRENT_TAB,
                                     ui::PAGE_TRANSITION_TYPED, false));
    auth_needed_waiter.Wait();
  }

  ASSERT_FALSE(observer.handlers().empty());
  {
    WindowedAuthNeededObserver auth_needed_waiter(controller);
    WindowedAuthSuppliedObserver auth_supplied_waiter(controller);
    LoginHandler* handler = *observer.handlers().begin();

    ASSERT_TRUE(handler);
    handler->SetAuth(base::UTF8ToUTF16(bad_username_),
                     base::UTF8ToUTF16(bad_password_));
    auth_supplied_waiter.Wait();

    // The request should be retried after the incorrect password is
    // supplied.  This should result in a new AUTH_NEEDED notification
    // for the same realm.
    auth_needed_waiter.Wait();
  }

  ASSERT_EQ(1u, observer.handlers().size());
  WindowedAuthSuppliedObserver auth_supplied_waiter(controller);
  LoginHandler* handler = *observer.handlers().begin();

  base::string16 username(base::UTF8ToUTF16(username_digest_));
  base::string16 password(base::UTF8ToUTF16(password_));
  handler->SetAuth(username, password);
  auth_supplied_waiter.Wait();

  WindowedLoadStopObserver load_stop_waiter(controller, 1);
  load_stop_waiter.Wait();

  base::string16 expected_title(base::UTF8ToUTF16("status=200"));

  EXPECT_EQ(expected_title, contents->GetTitle());
  EXPECT_EQ(2, observer.auth_supplied_count());
  EXPECT_EQ(2, observer.auth_needed_count());
  EXPECT_EQ(0, observer.auth_cancelled_count());
}

// If an XMLHttpRequest is made without credentials, there should be a login
// prompt.  If it's cancelled, the script should get a 401 status.
IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest,
                       LoginPromptForXHRWithoutCredentialsCancelled) {
  const char kXHRTestPage[] = "/login/xhr_without_credentials.html";

  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  NavigationController* controller = &contents->GetController();
  LoginPromptBrowserTestObserver observer;

  observer.Register(content::Source<NavigationController>(controller));

  // Load a page which makes a synchronous XMLHttpRequest for an authenticated
  // resource with the wrong credentials.  There should be no login prompt.
  {
    GURL test_page = embedded_test_server()->GetURL(kXHRTestPage);
    WindowedAuthNeededObserver auth_needed_waiter(controller);
    browser()->OpenURL(OpenURLParams(test_page, Referrer(),
                                     WindowOpenDisposition::CURRENT_TAB,
                                     ui::PAGE_TRANSITION_TYPED, false));
    auth_needed_waiter.Wait();
  }

  ASSERT_EQ(1u, observer.handlers().size());
  WindowedAuthCancelledObserver auth_cancelled_waiter(controller);
  LoginHandler* handler = *observer.handlers().begin();

  handler->CancelAuth();
  auth_cancelled_waiter.Wait();

  WindowedLoadStopObserver load_stop_waiter(controller, 1);
  load_stop_waiter.Wait();

  base::string16 expected_title(base::UTF8ToUTF16("status=401"));

  EXPECT_EQ(expected_title, contents->GetTitle());
  EXPECT_EQ(0, observer.auth_supplied_count());
  EXPECT_EQ(1, observer.auth_needed_count());
  EXPECT_EQ(1, observer.auth_cancelled_count());
}

// If a cross origin navigation triggers a login prompt, the destination URL
// should be shown in the omnibox.
void LoginPromptBrowserTest::TestCrossOriginPrompt(
    const GURL& visit_url,
    const std::string& expected_hostname,
    bool cancel_prompt) const {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  NavigationController* controller = &contents->GetController();
  LoginPromptBrowserTestObserver observer;

  observer.Register(content::Source<NavigationController>(controller));

  // Load a page which will trigger a login prompt.
  WindowedAuthNeededObserver auth_needed_waiter(controller);
  browser()->OpenURL(OpenURLParams(visit_url, Referrer(),
                                   WindowOpenDisposition::CURRENT_TAB,
                                   ui::PAGE_TRANSITION_TYPED, false));
  ASSERT_EQ(visit_url.host(), contents->GetVisibleURL().host());
  auth_needed_waiter.Wait();
  ASSERT_EQ(1u, observer.handlers().size());
  content::WaitForInterstitialAttach(contents);

  // The omnibox should show the correct origin for the new page when the
  // login prompt is shown.
  EXPECT_EQ(expected_hostname, contents->GetVisibleURL().host());
  EXPECT_TRUE(contents->ShowingInterstitialPage());
  EXPECT_EQ(LoginInterstitialDelegate::kTypeForTesting,
            contents->GetInterstitialPage()
                ->GetDelegateForTesting()
                ->GetTypeForTesting());

  if (cancel_prompt) {
    // Cancel and wait for the interstitial to detach.
    LoginHandler* handler = *observer.handlers().begin();
    content::RunTaskAndWaitForInterstitialDetach(
        contents, base::Bind(&LoginHandler::CancelAuth, handler));

    EXPECT_EQ(expected_hostname, contents->GetVisibleURL().host());
    EXPECT_FALSE(contents->ShowingInterstitialPage());
  }
}

// If a cross origin direct navigation triggers a login prompt, the login
// interstitial should be shown.
IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest,
                       ShowCorrectUrlForCrossOriginMainFrameRequests) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL test_page = embedded_test_server()->GetURL(kAuthBasicPage);
  ASSERT_EQ("127.0.0.1", test_page.host());
  std::string auth_host("127.0.0.1");
  TestCrossOriginPrompt(test_page, auth_host, true);
}

// If a cross origin redirect triggers a login prompt, the destination URL
// should be shown in the omnibox when the auth dialog is displayed.
IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest,
                       ShowCorrectUrlForCrossOriginMainFrameRedirects) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const char kTestPage[] = "/login/cross_origin.html";
  GURL test_page = embedded_test_server()->GetURL(kTestPage);
  ASSERT_EQ("127.0.0.1", test_page.host());
  std::string auth_host("www.a.com");
  TestCrossOriginPrompt(test_page, auth_host, true);
}

// Same as above, but instead of cancelling the prompt for www.a.com at the end,
// the page redirects to another page (www.b.com) that triggers an auth dialog.
// This should cancel the login interstitial for the first page (www.a.com),
// create a blank interstitial for second page (www.b.com) and show its URL in
// the omnibox.

// Fails occasionally on Mac. http://crbug.com/852703
#if defined(OS_MACOSX)
#define MAYBE_CancelLoginInterstitialOnRedirect \
  DISABLED_CancelLoginInterstitialOnRedirect
#else
#define MAYBE_CancelLoginInterstitialOnRedirect \
  CancelLoginInterstitialOnRedirect
#endif
IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest,
                       MAYBE_CancelLoginInterstitialOnRedirect) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // The test page redirects to www.a.com which triggers an auth dialog.
  const char kTestPage[] = "/login/cross_origin.html";
  GURL test_page = embedded_test_server()->GetURL(kTestPage);
  ASSERT_EQ("127.0.0.1", test_page.host());

  // The page at b.com simply displays an auth dialog.
  GURL::Replacements replace_host2;
  replace_host2.SetHostStr("www.b.com");
  GURL page2 = embedded_test_server()
                   ->GetURL(kAuthBasicPage)
                   .ReplaceComponents(replace_host2);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  NavigationController* controller = &contents->GetController();
  LoginPromptBrowserTestObserver observer;
  observer.Register(content::Source<NavigationController>(controller));

  // Load the test page. It should end up on www.a.com with the auth dialog
  // open.
  TestCrossOriginPrompt(test_page, "www.a.com", false);
  ASSERT_EQ(1u, observer.handlers().size());

  // While the auth dialog is open for www.a.com, redirect to www.b.com which
  // also triggers an auth dialog. This should cancel the auth dialog for
  // www.a.com and end up displaying an auth interstitial and the URL for
  // www.b.com.
  WindowedAuthCancelledObserver auth_cancelled_waiter(controller);
  EXPECT_TRUE(content::ExecuteScript(
      contents, std::string("document.location='") + page2.spec() + "';"));
  auth_cancelled_waiter.Wait();
  content::WaitForInterstitialDetach(contents);
  // Wait for the auth dialog and the interstitial for www.b.com.
  WindowedAuthNeededObserver auth_needed_waiter(controller);
  auth_needed_waiter.Wait();
  ASSERT_EQ(1u, observer.handlers().size());
  content::WaitForInterstitialAttach(contents);

  EXPECT_TRUE(contents->ShowingInterstitialPage());
  EXPECT_EQ(LoginInterstitialDelegate::kTypeForTesting,
            contents->GetInterstitialPage()
                ->GetDelegateForTesting()
                ->GetTypeForTesting());
  EXPECT_EQ("www.b.com", contents->GetVisibleURL().host());

  // Cancel auth dialog for www.b.com and wait for the interstitial to detach.
  LoginHandler* handler = *observer.handlers().begin();
  content::RunTaskAndWaitForInterstitialDetach(
      contents, base::Bind(&LoginHandler::CancelAuth, handler));
  EXPECT_EQ("www.b.com", contents->GetVisibleURL().host());
  EXPECT_FALSE(contents->ShowingInterstitialPage());
}

// Test the scenario where proceeding through a different type of interstitial
// that ends up with an auth URL works fine. This can happen if a URL that
// triggers the auth dialog can also trigger an SSL interstitial (or any other
// type of interstitial).
IN_PROC_BROWSER_TEST_F(
    LoginPromptBrowserTest,
    DISABLED_LoginInterstitialShouldReplaceExistingInterstitial) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  ASSERT_TRUE(https_server.Start());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  NavigationController* controller = &contents->GetController();
  LoginPromptBrowserTestObserver observer;

  observer.Register(content::Source<NavigationController>(controller));

  // Load a page which triggers an SSL interstitial. Proceeding through it
  // should show the login page with the blank interstitial.
  {
    GURL test_page = https_server.GetURL(kAuthBasicPage);
    ASSERT_EQ("127.0.0.1", test_page.host());

    WindowedAuthNeededObserver auth_needed_waiter(controller);
    browser()->OpenURL(OpenURLParams(test_page, Referrer(),
                                     WindowOpenDisposition::CURRENT_TAB,
                                     ui::PAGE_TRANSITION_TYPED, false));
    ASSERT_EQ("127.0.0.1", contents->GetURL().host());
    content::WaitForInterstitialAttach(contents);

    EXPECT_EQ(SSLBlockingPage::kTypeForTesting, contents->GetInterstitialPage()
                                                    ->GetDelegateForTesting()
                                                    ->GetTypeForTesting());
    // An overrideable SSL interstitial is now being displayed. Proceed through
    // the interstitial to see the login prompt.
    contents->GetInterstitialPage()->Proceed();
    auth_needed_waiter.Wait();
    ASSERT_EQ(1u, observer.handlers().size());
    content::WaitForInterstitialAttach(contents);

    // The omnibox should show the correct origin while the login prompt is
    // being displayed.
    EXPECT_EQ("127.0.0.1", contents->GetVisibleURL().host());
    EXPECT_TRUE(contents->ShowingInterstitialPage());
    EXPECT_EQ(LoginInterstitialDelegate::kTypeForTesting,
              contents->GetInterstitialPage()
                  ->GetDelegateForTesting()
                  ->GetTypeForTesting());

    // Cancelling the login prompt should detach the interstitial while keeping
    // the correct origin.
    LoginHandler* handler = *observer.handlers().begin();
    content::RunTaskAndWaitForInterstitialDetach(
        contents, base::Bind(&LoginHandler::CancelAuth, handler));

    EXPECT_EQ("127.0.0.1", contents->GetVisibleURL().host());
    EXPECT_FALSE(contents->ShowingInterstitialPage());
  }
}

// Test the scenario where an auth interstitial should replace a different type
// of interstitial (e.g. SSL) even though the navigation isn't cross origin.
// This is different than the above scenario in that the last
// committed url is the same as the auth url. This can happen when:
//
// 1. Tab is navigated to the auth URL and the auth prompt is cancelled.
// 2. Tab is then navigated to an SSL interstitial.
// 3. Tab is again navigated to the same auth URL in (1).
//
// In this case, the last committed url is the same as the auth URL since the
// navigation at (1) is committed (user clicked cancel and the page loaded), but
// the navigation at (2) isn't (navigations ending up in interstitials don't
// immediately commit). So just checking for cross origin navigation before
// prompting the auth interstitial is not sufficient, must also check if there
// is any other interstitial being displayed.
IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest,
                       ShouldReplaceExistingInterstitialWhenNavigated) {
  ASSERT_TRUE(embedded_test_server()->Start());
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  ASSERT_TRUE(https_server.Start());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  NavigationController* controller = &contents->GetController();
  LoginPromptBrowserTestObserver observer;

  observer.Register(content::Source<NavigationController>(controller));

  GURL auth_url = embedded_test_server()->GetURL(kAuthBasicPage);
  GURL broken_ssl_page = https_server.GetURL("/");

  // Navigate to an auth url and wait for the login prompt.
  {
    WindowedAuthNeededObserver auth_needed_waiter(controller);
    browser()->OpenURL(OpenURLParams(auth_url, Referrer(),
                                     WindowOpenDisposition::CURRENT_TAB,
                                     ui::PAGE_TRANSITION_TYPED, false));
    ASSERT_EQ("127.0.0.1", contents->GetURL().host());
    ASSERT_TRUE(contents->GetURL().SchemeIs("http"));
    auth_needed_waiter.Wait();
    ASSERT_EQ(1u, observer.handlers().size());
    content::WaitForInterstitialAttach(contents);
    ASSERT_TRUE(contents->ShowingInterstitialPage());
    EXPECT_EQ(LoginInterstitialDelegate::kTypeForTesting,
              contents->GetInterstitialPage()
                  ->GetDelegateForTesting()
                  ->GetTypeForTesting());
    // Cancel the auth prompt. This commits the navigation.
    LoginHandler* handler = *observer.handlers().begin();
    content::RunTaskAndWaitForInterstitialDetach(
        contents, base::Bind(&LoginHandler::CancelAuth, handler));
    EXPECT_EQ("127.0.0.1", contents->GetVisibleURL().host());
    EXPECT_FALSE(contents->ShowingInterstitialPage());
    EXPECT_EQ(auth_url, contents->GetLastCommittedURL());
  }

  // Navigate to a broken SSL page. This is a cross origin navigation since
  // schemes don't match (http vs https).
  {
    ASSERT_EQ("127.0.0.1", broken_ssl_page.host());
    browser()->OpenURL(OpenURLParams(broken_ssl_page, Referrer(),
                                     WindowOpenDisposition::CURRENT_TAB,
                                     ui::PAGE_TRANSITION_TYPED, false));
    ASSERT_EQ("127.0.0.1", contents->GetURL().host());
    ASSERT_TRUE(contents->GetURL().SchemeIs("https"));
    content::WaitForInterstitialAttach(contents);
    EXPECT_TRUE(contents->ShowingInterstitialPage());
    EXPECT_EQ(SSLBlockingPage::kTypeForTesting, contents->GetInterstitialPage()
                                                    ->GetDelegateForTesting()
                                                    ->GetTypeForTesting());
    EXPECT_EQ(auth_url, contents->GetLastCommittedURL());
  }

  // An overrideable SSL interstitial is now being displayed. Navigate to the
  // auth URL again. This is again a cross origin navigation, but last committed
  // URL is the same as the auth URL (since SSL navigation never committed).
  // Should still replace SSL interstitial with an auth interstitial even though
  // last committed URL and the new URL is the same.
  {
    WindowedAuthNeededObserver auth_needed_waiter(controller);
    browser()->OpenURL(OpenURLParams(auth_url, Referrer(),
                                     WindowOpenDisposition::CURRENT_TAB,
                                     ui::PAGE_TRANSITION_TYPED, false));
    ASSERT_EQ("127.0.0.1", contents->GetURL().host());
    ASSERT_TRUE(contents->GetURL().SchemeIs("http"));
    ASSERT_TRUE(contents->ShowingInterstitialPage());

    auth_needed_waiter.Wait();
    ASSERT_EQ(1u, observer.handlers().size());
    content::WaitForInterstitialAttach(contents);
    EXPECT_EQ(LoginInterstitialDelegate::kTypeForTesting,
              contents->GetInterstitialPage()
                  ->GetDelegateForTesting()
                  ->GetTypeForTesting());
  }
}

// Test that the login interstitial isn't proceeding itself or any other
// interstitial. If this test becomes flaky, it's likely that the logic that
// prevents the tested scenario from happening got broken, rather than the test
// itself.
IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest,
                       ShouldNotProceedExistingInterstitial) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  ASSERT_TRUE(https_server.Start());

  const char* kTestPage = "/login/load_iframe_from_b.html";

  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  NavigationController* controller = &contents->GetController();
  LoginPromptBrowserTestObserver observer;
  observer.Register(content::Source<NavigationController>(controller));

  // Load a page that has a cross-domain iframe authentication. This should
  // trigger a login prompt but no login interstitial.
  GURL test_page = embedded_test_server()->GetURL(kTestPage);
  GURL broken_ssl_page = https_server.GetURL("/");
  ASSERT_EQ("127.0.0.1", test_page.host());
  WindowedAuthNeededObserver auth_needed_waiter(controller);
  browser()->OpenURL(OpenURLParams(test_page, Referrer(),
                                   WindowOpenDisposition::CURRENT_TAB,
                                   ui::PAGE_TRANSITION_TYPED, false));
  auth_needed_waiter.Wait();
  ASSERT_EQ(1u, observer.handlers().size());
  EXPECT_FALSE(contents->ShowingInterstitialPage());

  // Redirect to a broken SSL page. This redirect should not accidentally
  // proceed through the SSL interstitial.
  WindowedAuthCancelledObserver auth_cancelled_waiter(controller);
  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      std::string("window.location = '") + broken_ssl_page.spec() + "'"));
  content::WaitForInterstitialAttach(contents);
  auth_cancelled_waiter.Wait();

  // If the interstitial was accidentally clicked through, this wait may time
  // out.
  EXPECT_TRUE(
      WaitForRenderFrameReady(contents->GetInterstitialPage()->GetMainFrame()));
  EXPECT_TRUE(contents->ShowingInterstitialPage());
  EXPECT_EQ(SSLBlockingPage::kTypeForTesting, contents->GetInterstitialPage()
                                                  ->GetDelegateForTesting()
                                                  ->GetTypeForTesting());
}

// Test where Basic HTTP authentication is disabled.
IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest, PRE_TestBasicAuthDisabled) {
  // Disable all auth schemes. The modified list isn't respected until the
  // browser is restarted, however.
  g_browser_process->local_state()->SetString(prefs::kAuthSchemes, "");
}

IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest, TestBasicAuthDisabled) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_page = embedded_test_server()->GetURL(kAuthBasicPage);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  NavigationController* controller = &contents->GetController();

  // If the network service crashes, basic auth should still be disabled.
  for (bool crash_network_service : {false, true}) {
    // Crash the network service if it is enabled.
    if (crash_network_service &&
        base::FeatureList::IsEnabled(network::features::kNetworkService)) {
      SimulateNetworkServiceCrash();
      // Flush the network interface to make sure it notices the crash.
      content::BrowserContext::GetDefaultStoragePartition(browser()->profile())
          ->FlushNetworkInterfaceForTesting();
    }

    LoginPromptBrowserTestObserver observer;

    observer.Register(content::Source<NavigationController>(controller));
    browser()->OpenURL(OpenURLParams(test_page, Referrer(),
                                     WindowOpenDisposition::CURRENT_TAB,
                                     ui::PAGE_TRANSITION_TYPED, false));
    EXPECT_EQ(0, observer.auth_supplied_count());

    const base::string16 kExpectedTitle =
        base::ASCIIToUTF16("Denied: Missing Authorization Header");
    content::TitleWatcher title_watcher(contents, kExpectedTitle);
    EXPECT_EQ(kExpectedTitle, title_watcher.WaitAndGetTitle());
  }
}

}  // namespace
