// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/login/login_handler.h"

#include <list>
#include <map>
#include <tuple>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/field_trial.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/net/proxy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/login/http_auth_coordinator.h"
#include "chrome/browser/ui/login/login_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/content/ssl_blocking_page.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/slow_http_response.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/process_manager.h"
#include "net/base/auth.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/extension_features.h"
#endif

using content::NavigationController;
using content::OpenURLParams;
using content::Referrer;

namespace {

// A slow HTTP response that serves a WWW-Authenticate header and 401 status
// code.
class SlowAuthResponse : public content::SlowHttpResponse {
 public:
  explicit SlowAuthResponse(GotRequestCallback got_request)
      : content::SlowHttpResponse(std::move(got_request)) {}
  ~SlowAuthResponse() override = default;

  SlowAuthResponse(const SlowAuthResponse& other) = delete;
  SlowAuthResponse operator=(const SlowAuthResponse& other) = delete;

  // content::SlowHttpResponse:
  base::StringPairs ResponseHeaders() override {
    base::StringPairs response;
    response.emplace_back("WWW-Authenticate", "Basic realm=\"test\"");
    response.emplace_back("Cache-Control", "max-age=0");
    // Content-length and Content-type are both necessary to trigger the bug
    // that this class is used to test. Specifically, there must be a delay
    // between the OnAuthRequired notification from the net stack and when the
    // response body is ready, and the OnAuthRequired notification requires
    // headers to be complete (which requires a known content type and length).
    response.emplace_back("Content-type", "text/html");
    response.emplace_back(
        "Content-Length",
        base::NumberToString(kFirstResponsePartSize + kSecondResponsePartSize));
    return response;
  }

  std::pair<net::HttpStatusCode, std::string> StatusLine() override {
    return {net::HTTP_UNAUTHORIZED, "Unauthorized"};
  }
};

security_interstitials::SecurityInterstitialPage* GetSecurityInterstitial(
    content::WebContents* tab) {
  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          tab);
  if (!helper) {
    return nullptr;
  }
  return helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting();
}

enum class SplitAuthCacheByNetworkIsolationKey {
  kFalse,
  kTrue,
};

class BrowserClientFake;
class LoginHandlerFake : public LoginHandler {
 public:
  LoginHandlerFake(const net::AuthChallengeInfo& auth_info,
                   content::WebContents* web_contents,
                   LoginAuthRequiredCallback auth_required_callback,
                   BrowserClientFake* browser_client)
      : LoginHandler(auth_info,
                     web_contents,
                     std::move(auth_required_callback)),
        browser_client_(browser_client) {}

  ~LoginHandlerFake() override;

  void NotifyAuthNeeded() override;
  void NotifyAuthSupplied(const std::u16string& username,
                          const std::u16string& password) override;
  void NotifyAuthCancelled() override;
  bool BuildViewImpl(const std::u16string& authority,
                     const std::u16string& explanation,
                     LoginModelData* login_model_data) override {
    return true;
  }
  void CloseDialog() override {}
  const raw_ptr<BrowserClientFake> browser_client_;
  bool prompt_shown_ = false;
  bool prompt_responded_ = false;
};

class LoginTabHelperFake : public LoginTabHelper {
 public:
  LoginTabHelperFake(content::WebContents* web_contents,
                     BrowserClientFake* browser_client)
      : LoginTabHelper(web_contents), browser_client_(browser_client) {}

  std::unique_ptr<LoginHandler> CreateLoginHandler(
      const net::AuthChallengeInfo& auth_info,
      content::WebContents* web_contents,
      LoginAuthRequiredCallback auth_required_callback) override {
    return std::make_unique<LoginHandlerFake>(auth_info, web_contents,
                                              std::move(auth_required_callback),
                                              browser_client_);
  }

  const raw_ptr<BrowserClientFake> browser_client_;
};

class HttpAuthCoordinatorFake : public HttpAuthCoordinator {
 public:
  explicit HttpAuthCoordinatorFake(BrowserClientFake* browser_client)
      : browser_client_(browser_client) {}
  ~HttpAuthCoordinatorFake() override = default;

  void CreateLoginTabHelper(content::WebContents* web_contents) override {
    if (!LoginTabHelper::FromWebContents(web_contents)) {
      auto tab_helper =
          std::make_unique<LoginTabHelperFake>(web_contents, browser_client_);
      web_contents->SetUserData(LoginTabHelper::UserDataKey(),
                                std::move(tab_helper));
    }
  }

  std::unique_ptr<content::LoginDelegate> CreateLoginDelegateFromLoginHandler(
      content::WebContents* web_contents,
      const net::AuthChallengeInfo& auth_info,
      const content::GlobalRequestID& request_id,
      const GURL& url,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      content::LoginDelegate::LoginAuthRequiredCallback auth_required_callback)
      override {
    auto login_handler = std::make_unique<LoginHandlerFake>(
        auth_info, web_contents, std::move(auth_required_callback),
        browser_client_);
    login_handler->ShowLoginPrompt(url);
    return login_handler;
  }

 private:
  const raw_ptr<BrowserClientFake> browser_client_;
};

class BrowserClientFake : public ChromeContentBrowserClient {
 public:
  std::unique_ptr<HttpAuthCoordinator> CreateHttpAuthCoordinator() override {
    return std::make_unique<HttpAuthCoordinatorFake>(this);
  }

  // This is needed for tests that restart the network service.
  std::vector<base::FilePath> GetNetworkContextsParentDirectory() override {
    base::FilePath user_data_dir =
        base::PathService::CheckedGet(chrome::DIR_USER_DATA);
    return {user_data_dir};
  }
  int auth_needed_count = 0;
  int auth_supplied_count = 0;
  int auth_cancelled_count = 0;
};

LoginHandlerFake::~LoginHandlerFake() {
  // The tests rely on the assumption that a login handler that was shown and
  // then destroyed is functionally equivalent to "cancelled", even though no
  // notification is sent. This could be replaced with auth_abandoned_count but
  // would require a careful refactor of all the tests.
  if (prompt_shown_ && !prompt_responded_) {
    browser_client_->auth_cancelled_count++;
  }
}

void LoginHandlerFake::NotifyAuthNeeded() {
  browser_client_->auth_needed_count++;
  prompt_shown_ = true;
  LoginHandler::NotifyAuthNeeded();
}
void LoginHandlerFake::NotifyAuthSupplied(const std::u16string& username,
                                          const std::u16string& password) {
  browser_client_->auth_supplied_count++;
  prompt_responded_ = true;
  LoginHandler::NotifyAuthSupplied(username, password);
}
void LoginHandlerFake::NotifyAuthCancelled() {
  browser_client_->auth_cancelled_count++;
  prompt_responded_ = true;
  LoginHandler::NotifyAuthCancelled();
}

// Waits for an int member of another class to increment.
class AuthObserver {
 public:
  explicit AuthObserver(int* member)
      : member_(member), initial_value_(*member) {}

  void Wait() {
    ASSERT_TRUE(
        base::test::RunUntil([&]() { return *member_ > initial_value_; }));
  }

 private:
  const raw_ptr<int> member_;
  const int initial_value_;
};

// Contains helper methods shared across different subclasses of
// InProcessBrowserTest.
class LoginPromptBrowserTestHelper {
 public:
  void SetUpLoginFakes() {
    browser_client_ = std::make_unique<BrowserClientFake>();
    content::SetBrowserClientForTesting(browser_client_.get());
  }
  AuthObserver CreateAuthNeededObserver() {
    return AuthObserver(&browser_client_->auth_needed_count);
  }
  AuthObserver CreateAuthCancelledObserver() {
    return AuthObserver(&browser_client_->auth_cancelled_count);
  }
  AuthObserver CreateAuthSuppliedObserver() {
    return AuthObserver(&browser_client_->auth_supplied_count);
  }
  std::unique_ptr<BrowserClientFake> browser_client_;
};

class LoginPromptBrowserTest
    : public InProcessBrowserTest,
      public LoginPromptBrowserTestHelper,
      public testing::WithParamInterface<SplitAuthCacheByNetworkIsolationKey> {
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

    scoped_feature_list_.InitWithFeatureStates(
        {{network::features::kSplitAuthCacheByNetworkIsolationKey,
          (GetParam() == SplitAuthCacheByNetworkIsolationKey::kTrue)}});
  }

  void SetUpOnMainThread() override {
    SetUpLoginFakes();
    host_resolver()->AddRule("*", "127.0.0.1");
    InProcessBrowserTest::SetUpOnMainThread();
  }

  // Tests that a cross origin navigation triggering a login prompt should
  // cause:
  // - A login interstitial being displayed.
  // - The destination URL being shown in the omnibox.
  // Navigates to |visit_url| which triggers an HTTP auth dialog, and checks if
  // the URL displayed in the omnibox is equal to |expected_url| after all
  // navigations including page redirects are completed.
  // If |cancel_prompt| is true, the auth dialog is cancelled at the end.
  void TestCrossOriginPrompt(Browser* browser,
                             const GURL& visit_url,
                             const std::string& expected_hostname,
                             bool cancel_prompt) {
    content::WebContents* contents =
        browser->tab_strip_model()->GetActiveWebContents();

    // Load a page which will trigger a login prompt.
    auto auth_needed_waiter = CreateAuthNeededObserver();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, visit_url));
    auth_needed_waiter.Wait();
    ASSERT_EQ(1u, LoginHandler::GetAllLoginHandlersForTest().size());

    // The omnibox should show the correct origin for the new page when the
    // login prompt is shown.
    EXPECT_EQ(expected_hostname, contents->GetVisibleURL().host());

    if (cancel_prompt) {
      // Cancel, which triggers a reload to get the error page content from the
      // server.
      LoginHandler* handler =
          LoginHandler::GetAllLoginHandlersForTest().front();
      content::TestNavigationObserver reload_observer(contents);
      handler->CancelAuth(/*notify_others=*/true);
      reload_observer.Wait();
      EXPECT_EQ(expected_hostname, contents->GetVisibleURL().host());
    }
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
  // Authenticates for BasicAuth and waits the authentication is accepted.
  void SetAuthForAndWait(LoginHandler* handler);
  // Waits until the title matches the expected title for BasicAuth.
  void ExpectSuccessfulBasicAuthTitle(content::WebContents* contents);

  AuthMap auth_map_;
  std::string bad_password_;
  std::string bad_username_;
  std::string password_;
  std::string username_basic_;
  std::string username_digest_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    LoginPromptBrowserTest,
    ::testing::Values(SplitAuthCacheByNetworkIsolationKey::kFalse,
                      SplitAuthCacheByNetworkIsolationKey::kTrue));

void LoginPromptBrowserTest::SetAuthFor(LoginHandler* handler) {
  const net::AuthChallengeInfo& challenge = handler->auth_info();

  auto i = auth_map_.find(challenge.realm);
  EXPECT_TRUE(auth_map_.end() != i);
  if (i != auth_map_.end()) {
    const AuthInfo& info = i->second;
    handler->SetAuth(base::UTF8ToUTF16(info.username_),
                     base::UTF8ToUTF16(info.password_));
  }
}

void LoginPromptBrowserTest::SetAuthForAndWait(LoginHandler* handler) {
  ASSERT_TRUE(handler);
  auto auth_supplied_waiter = CreateAuthSuppliedObserver();
  SetAuthFor(handler);
  auth_supplied_waiter.Wait();
}

std::u16string ExpectedTitleFromAuth(const std::u16string& username,
                                     const std::u16string& password) {
  // The TestServer sets the title to username/password on successful login.
  return username + u"/" + password;
}

void LoginPromptBrowserTest::ExpectSuccessfulBasicAuthTitle(
    content::WebContents* contents) {
  std::u16string expected_title =
      ExpectedTitleFromAuth(u"basicuser", u"secret");
  content::TitleWatcher title_watcher(contents, expected_title);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

// There are tests that will fail with third party cookies blocked. To ensure
// that there is no regression in behavior when third party cookies are not
// blocked this test fixture has been added which turns off third party cookie
// blocking.
// crbug/1503201 - LoginPromptBrowserTest cases fail in 3PCD
class LoginPromptBrowserTestThirdPartyCookiesUnblocked
    : public LoginPromptBrowserTest {
 public:
  LoginPromptBrowserTestThirdPartyCookiesUnblocked() {
    scoped_feature_list_.InitWithFeatureStates(
        {{network::features::kSplitAuthCacheByNetworkIsolationKey,
          (GetParam() == SplitAuthCacheByNetworkIsolationKey::kTrue)},
         {content_settings::features::kTrackingProtection3pcd, false}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    LoginPromptBrowserTestThirdPartyCookiesUnblocked,
    ::testing::Values(SplitAuthCacheByNetworkIsolationKey::kFalse,
                      SplitAuthCacheByNetworkIsolationKey::kTrue));

const char kPrefetchAuthPage[] = "/login/prefetch.html";

const char kMultiRealmTestPage[] = "/login/multi_realm.html";
const int  kMultiRealmTestRealmCount = 2;
const int kMultiRealmTestAuthRequestsCount = 4;

const char kAuthBasicPage[] = "/auth-basic";
const char kAuthBasicSubframePage[] = "/auth-basic-subframe.html";
const char kAuthDigestPage[] = "/auth-digest";

const char kTitlePage[] = "/title1.html";
const char kCCNSPage[] = "/echoall/nocache";

// It does not matter what pages are selected as no-auth, as long as they exist.
// Navigating to non-existing pages caused flakes in the past
// (https://crbug.com/636875).
const char kNoAuthPage1[] = "/simple.html";

// Confirm that <link rel="prefetch"> targetting an auth required
// resource does not provide a login dialog.  These types of requests
// should instead just cancel the auth.

// Unfortunately, this test doesn't assert on anything for its
// correctness.  Instead, it relies on the auth dialog blocking the
// browser, and triggering a timeout to cause failure when the
// prefetch resource requires authorization.
IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTest, PrefetchAuthCancels) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_page = embedded_test_server()->GetURL(kPrefetchAuthPage);

  class SetPrefetchForTest {
   public:
    explicit SetPrefetchForTest(bool prefetch) {
      std::string exp_group = prefetch ? "ExperimentYes" : "ExperimentNo";
      base::FieldTrialList::CreateFieldTrial("Prefetch", exp_group);
    }
    ~SetPrefetchForTest() = default;
  } set_prefetch_for_test(true);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page));
  EXPECT_TRUE(LoginHandler::GetAllLoginHandlersForTest().empty());
}

// Test that "Basic" HTTP authentication works.
IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTest, TestBasicAuth) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_page = embedded_test_server()->GetURL(kAuthBasicPage);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // If the network service crashes, basic auth should still be enabled.
  for (bool crash_network_service : {false, true}) {
    if (crash_network_service) {
      // Can't crash the network service if it isn't running out of process.
      if (!content::IsOutOfProcessNetworkService())
        return;

      SimulateNetworkServiceCrash();
      // Flush the network interface to make sure it notices the crash.
      browser()
          ->profile()
          ->GetDefaultStoragePartition()
          ->FlushNetworkInterfaceForTesting();
    }

    {
      auto auth_needed_waiter = CreateAuthNeededObserver();
      browser()->OpenURL(OpenURLParams(test_page, Referrer(),
                                       WindowOpenDisposition::CURRENT_TAB,
                                       ui::PAGE_TRANSITION_TYPED, false),
                         /*navigation_handle_callback=*/{});
      auth_needed_waiter.Wait();
    }

    ASSERT_FALSE(LoginHandler::GetAllLoginHandlersForTest().empty());
    {
      auto auth_needed_waiter = CreateAuthNeededObserver();
      auto auth_supplied_waiter = CreateAuthSuppliedObserver();
      LoginHandler* handler =
          LoginHandler::GetAllLoginHandlersForTest().front();

      ASSERT_TRUE(handler);
      handler->SetAuth(base::UTF8ToUTF16(bad_username_),
                       base::UTF8ToUTF16(bad_password_));
      auth_supplied_waiter.Wait();

      // The request should be retried after the incorrect password is
      // supplied.  This should result in a new AUTH_NEEDED notification
      // for the same realm.
      auth_needed_waiter.Wait();
    }

    ASSERT_EQ(1u, LoginHandler::GetAllLoginHandlersForTest().size());
    SetAuthForAndWait(LoginHandler::GetAllLoginHandlersForTest().front());
    ExpectSuccessfulBasicAuthTitle(contents);
  }
}

// Test that a BasicAuth prompt from the main frame prevents the page from
// entering back/forward cache but that a successful authentication does not.
IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTest,
                       TestBasicAuthPromptBlocksBackForwardCache) {
  // Don't run this test if BackForwardCache is disabled.
  if (!content::BackForwardCache::IsBackForwardCacheFeatureEnabled()) {
    return;
  }
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL test_page = embedded_test_server()->GetURL(kAuthBasicPage);
  GURL title_page = embedded_test_server()->GetURL("a.com", kTitlePage);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  NavigationController* controller = &contents->GetController();

  // Navigate to the page and wait for the auth prompt.
  {
    auto auth_needed_waiter = CreateAuthNeededObserver();
    browser()->OpenURL(
        OpenURLParams(test_page, Referrer(), WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false),
        /*navigation_handle_callback=*/{});
    auth_needed_waiter.Wait();
  }
  content::RenderFrameHostWrapper rfh(contents->GetPrimaryMainFrame());

  // Navigate away.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), title_page));

  // We expect the previous page to be destroyed without entering back/forward
  // cache.
  ASSERT_TRUE(rfh.WaitUntilRenderFrameDeleted());

  // Go back to the page and wait for the auth prompt.
  {
    auto auth_needed_waiter = CreateAuthNeededObserver();
    controller->GoBack();
    auth_needed_waiter.Wait();
  }

  // Complete the authentication.
  SetAuthForAndWait(LoginHandler::GetAllLoginHandlersForTest().front());
  ExpectSuccessfulBasicAuthTitle(contents);

  // Navigate away and go back again.
  content::RenderFrameHostWrapper rfh2(contents->GetPrimaryMainFrame());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), title_page));
  ASSERT_TRUE(content::HistoryGoBack(contents));

  // This time the page should have been restored from the cache.
  ASSERT_EQ(rfh2.get(), contents->GetPrimaryMainFrame());
}

// Test that a BasicAuth prompt from a subframe prevents the page from
// entering back/forward cache.
IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTest,
                       TestBasicAuthPromptSubframeBlocksBackForwardCache) {
  // Don't run this test if BackForwardCache is disabled.
  if (!content::BackForwardCache::IsBackForwardCacheFeatureEnabled()) {
    return;
  }
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL test_page = embedded_test_server()->GetURL(kAuthBasicSubframePage);
  GURL title_page = embedded_test_server()->GetURL("a.com", kTitlePage);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  NavigationController* controller = &contents->GetController();

  // Navigate to the page and wait for the auth prompt.
  {
    auto auth_needed_waiter = CreateAuthNeededObserver();
    browser()->OpenURL(
        OpenURLParams(test_page, Referrer(), WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false),
        /*navigation_handle_callback=*/{});
    auth_needed_waiter.Wait();
  }
  content::RenderFrameHostWrapper rfh(contents->GetPrimaryMainFrame());

  // Navigate away.
  // TODO(crbug.com/40267658): Use `NavigateToURL`.
  auto auth_cancelled_waiter = CreateAuthCancelledObserver();
  ASSERT_TRUE(content::NavigateToURLFromRenderer(contents, title_page));
  auth_cancelled_waiter.Wait();

  // We expect the previous page to be destroyed without entering back/forward
  // cache.
  ASSERT_TRUE(rfh.WaitUntilRenderFrameDeleted());

  // Go back to the page and wait for the auth prompt.
  {
    auto auth_needed_waiter = CreateAuthNeededObserver();
    controller->GoBack();
    auth_needed_waiter.Wait();
  }

  // Complete the authentication.
  SetAuthForAndWait(LoginHandler::GetAllLoginHandlersForTest().front());
  content::WaitForLoadStop(contents);
  ASSERT_EQ(ExpectedTitleFromAuth(u"basicuser", u"secret"),
            content::EvalJs(contents, "subframe.contentDocument.title"));

  // Navigate away and go back again.
  content::RenderFrameHostWrapper rfh2(contents->GetPrimaryMainFrame());
  // TODO(crbug.com/40267658): Use `NavigateToURL`.
  ASSERT_TRUE(content::NavigateToURLFromRenderer(contents, title_page));
  ASSERT_TRUE(content::HistoryGoBack(contents));

  // This time the page should have been restored from the cache.
  ASSERT_EQ(rfh2.get(), contents->GetPrimaryMainFrame());
}

// Test that "Digest" HTTP authentication works.
IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTest, TestDigestAuth) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_page = embedded_test_server()->GetURL(kAuthDigestPage);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  {
    auto auth_needed_waiter = CreateAuthNeededObserver();
    browser()->OpenURL(
        OpenURLParams(test_page, Referrer(), WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false),
        /*navigation_handle_callback=*/{});
    auth_needed_waiter.Wait();
  }

  ASSERT_FALSE(LoginHandler::GetAllLoginHandlersForTest().empty());
  {
    auto auth_needed_waiter = CreateAuthNeededObserver();
    auto auth_supplied_waiter = CreateAuthSuppliedObserver();
    LoginHandler* handler = LoginHandler::GetAllLoginHandlersForTest().front();

    ASSERT_TRUE(handler);
    handler->SetAuth(base::UTF8ToUTF16(bad_username_),
                     base::UTF8ToUTF16(bad_password_));
    auth_supplied_waiter.Wait();

    // The request should be retried after the incorrect password is
    // supplied.  This should result in a new AUTH_NEEDED notification
    // for the same realm.
    auth_needed_waiter.Wait();
  }

  ASSERT_EQ(1u, LoginHandler::GetAllLoginHandlersForTest().size());
  auto auth_supplied_waiter = CreateAuthSuppliedObserver();
  LoginHandler* handler = LoginHandler::GetAllLoginHandlersForTest().front();

  std::u16string username(base::UTF8ToUTF16(username_digest_));
  std::u16string password(base::UTF8ToUTF16(password_));
  handler->SetAuth(username, password);
  auth_supplied_waiter.Wait();

  std::u16string expected_title = ExpectedTitleFromAuth(username, password);
  content::TitleWatcher title_watcher(contents, expected_title);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTest, TestTwoAuths) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* contents1 =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Open a new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  content::WebContents* contents2 =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(contents1, contents2);

  {
    auto auth_needed_waiter = CreateAuthNeededObserver();
    contents1->OpenURL(
        OpenURLParams(embedded_test_server()->GetURL(kAuthBasicPage),
                      Referrer(), WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false),
        /*navigation_handle_callback=*/{});
    auth_needed_waiter.Wait();
  }

  {
    auto auth_needed_waiter = CreateAuthNeededObserver();
    contents2->OpenURL(
        OpenURLParams(embedded_test_server()->GetURL(kAuthDigestPage),
                      Referrer(), WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false),
        /*navigation_handle_callback=*/{});
    auth_needed_waiter.Wait();
  }

  ASSERT_EQ(2u, LoginHandler::GetAllLoginHandlersForTest().size());

  LoginHandler* handler1 = LoginHandler::GetAllLoginHandlersForTest().front();
  LoginHandler* handler2 =
      *(++(LoginHandler::GetAllLoginHandlersForTest().begin()));

  std::u16string expected_title1 = ExpectedTitleFromAuth(
      base::UTF8ToUTF16(username_basic_), base::UTF8ToUTF16(password_));
  std::u16string expected_title2 = ExpectedTitleFromAuth(
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
IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTest, TestCancelAuth_Manual) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kAuthURL = embedded_test_server()->GetURL(kAuthBasicPage);

  auto auth_needed_waiter = CreateAuthNeededObserver();
  browser()->OpenURL(
      OpenURLParams(kAuthURL, Referrer(), WindowOpenDisposition::CURRENT_TAB,
                    ui::PAGE_TRANSITION_TYPED, false),
      /*navigation_handle_callback=*/{});
  auth_needed_waiter.Wait();
  auto auth_cancelled_waiter = CreateAuthCancelledObserver();
  LoginHandler* handler = LoginHandler::GetAllLoginHandlersForTest().front();
  ASSERT_TRUE(handler);
  content::TestNavigationObserver reload_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  handler->CancelAuth(/*notify_others=*/true);
  auth_cancelled_waiter.Wait();
  reload_observer.Wait();
  EXPECT_TRUE(LoginHandler::GetAllLoginHandlersForTest().empty());
}

// Test login prompt cancellation on navigation to a new page.
IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTest, TestCancelAuth_OnNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kAuthURL = embedded_test_server()->GetURL(kAuthBasicPage);
  const GURL kNoAuthURL = embedded_test_server()->GetURL(kNoAuthPage1);

  // One LOAD_STOP event for kAuthURL and second for kNoAuthURL.
  auto auth_needed_waiter = CreateAuthNeededObserver();
  browser()->OpenURL(
      OpenURLParams(kAuthURL, Referrer(), WindowOpenDisposition::CURRENT_TAB,
                    ui::PAGE_TRANSITION_TYPED, false),
      /*navigation_handle_callback=*/{});
  auth_needed_waiter.Wait();
  auto auth_cancelled_waiter = CreateAuthCancelledObserver();
  // Navigating while auth is requested is the same as cancelling.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kNoAuthURL));
  auth_cancelled_waiter.Wait();
  EXPECT_TRUE(LoginHandler::GetAllLoginHandlersForTest().empty());
}

// Test login prompt cancellation on navigation to back.
IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTest, TestCancelAuth_OnBack) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kAuthURL = embedded_test_server()->GetURL(kAuthBasicPage);
  const GURL kNoAuthURL = embedded_test_server()->GetURL(kNoAuthPage1);

  NavigationController* controller =
      &browser()->tab_strip_model()->GetActiveWebContents()->GetController();

  // First navigate to an unauthenticated page so we have something to
  // go back to.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kNoAuthURL));

  auto auth_needed_waiter = CreateAuthNeededObserver();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kAuthURL));
  auth_needed_waiter.Wait();
  auto auth_cancelled_waiter = CreateAuthCancelledObserver();
  // Navigating back while auth is requested is the same as cancelling.
  ASSERT_TRUE(controller->CanGoBack());
  controller->GoBack();
  auth_cancelled_waiter.Wait();
  EXPECT_TRUE(LoginHandler::GetAllLoginHandlersForTest().empty());
}

// Test login prompt cancellation on navigation to forward.
IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTest, TestCancelAuth_OnForward) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kAuthURL = embedded_test_server()->GetURL(kAuthBasicPage);
  const GURL kNoAuthURL1 = embedded_test_server()->GetURL(kNoAuthPage1);

  NavigationController* controller =
      &browser()->tab_strip_model()->GetActiveWebContents()->GetController();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kAuthURL));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kNoAuthURL1));
  ASSERT_TRUE(controller->CanGoBack());
  auto auth_needed_waiter = CreateAuthNeededObserver();
  controller->GoBack();
  auth_needed_waiter.Wait();

  // Go forward and test that the login prompt is cancelled.
  auto auth_cancelled_waiter = CreateAuthCancelledObserver();
  ASSERT_TRUE(controller->CanGoForward());
  controller->GoForward();
  auth_cancelled_waiter.Wait();
  EXPECT_TRUE(LoginHandler::GetAllLoginHandlersForTest().empty());
}

// Test handling of resources that require authentication even though
// the page they are included on doesn't.  In this case we should only
// present the minimal number of prompts necessary for successfully
// displaying the page.
class MultiRealmLoginPromptBrowserTest : public LoginPromptBrowserTest {
 public:
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
};

INSTANTIATE_TEST_SUITE_P(
    All,
    MultiRealmLoginPromptBrowserTest,
    ::testing::Values(SplitAuthCacheByNetworkIsolationKey::kFalse,
                      SplitAuthCacheByNetworkIsolationKey::kTrue));

template <class F>
void MultiRealmLoginPromptBrowserTest::RunTest(const F& for_each_realm_func) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_page = embedded_test_server()->GetURL(kMultiRealmTestPage);

  NavigateParams params(browser(), test_page, ui::PAGE_TRANSITION_TYPED);
  Navigate(&params);

  // Need to have LoginHandlers created for all requests that need
  // authentication.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return LoginHandler::GetAllLoginHandlersForTest().size() ==
           kMultiRealmTestAuthRequestsCount;
  }));

  // Now confirm or cancel auth once per realm.
  std::set<std::string> seen_realms;
  for (int i = 0; i < kMultiRealmTestRealmCount; ++i) {
    auto handlers = LoginHandler::GetAllLoginHandlersForTest();
    auto it =
        base::ranges::find_if(handlers, [&seen_realms](LoginHandler* handler) {
          return seen_realms.count(handler->auth_info().realm) == 0;
        });
    ASSERT_TRUE(it != handlers.end());
    seen_realms.insert((*it)->auth_info().realm);

    for_each_realm_func(*it);
  }

  content::WaitForLoadStop(params.navigated_or_inserted_contents);
}

// Checks that cancelling works as expected.
IN_PROC_BROWSER_TEST_P(MultiRealmLoginPromptBrowserTest,
                       MultipleRealmCancellation) {
  RunTest([this](LoginHandler* handler) {
    auto waiter = CreateAuthCancelledObserver();
    handler->CancelAuth(/*notify_others=*/true);
    waiter.Wait();
  });

  EXPECT_EQ(0, browser_client_->auth_supplied_count);
  EXPECT_LT(0, browser_client_->auth_needed_count);
  EXPECT_LT(0, browser_client_->auth_cancelled_count);
}

// Checks that supplying credentials works as expected.
IN_PROC_BROWSER_TEST_P(MultiRealmLoginPromptBrowserTest,
                       MultipleRealmConfirmation) {
  RunTest([this](LoginHandler* handler) { SetAuthForAndWait(handler); });

  EXPECT_LT(0, browser_client_->auth_supplied_count);
  EXPECT_LT(0, browser_client_->auth_needed_count);
  EXPECT_EQ(0, browser_client_->auth_cancelled_count);
}

// If the favicon is an authenticated resource, we shouldn't prompt
// for credentials.  The same URL, if requested elsewhere should
// prompt for credentials.
IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTest, NoLoginPromptForFavicon) {
  const char kFaviconTestPage[] = "/login/has_favicon.html";
  const char kFaviconResource[] = "/auth-basic/favicon.gif";

  ASSERT_TRUE(embedded_test_server()->Start());

  // First load a page that has a favicon that requires
  // authentication.  There should be no login prompt.
  {
    GURL test_page = embedded_test_server()->GetURL(kFaviconTestPage);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page));
  }

  // Now request the same favicon, but directly as the document.
  // There should be one login prompt.
  {
    GURL test_page = embedded_test_server()->GetURL(kFaviconResource);
    auto auth_needed_waiter = CreateAuthNeededObserver();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page));
    auth_needed_waiter.Wait();
    ASSERT_EQ(1u, LoginHandler::GetAllLoginHandlersForTest().size());

    while (!LoginHandler::GetAllLoginHandlersForTest().empty()) {
      auto auth_cancelled_waiter = CreateAuthCancelledObserver();
      LoginHandler* handler =
          LoginHandler::GetAllLoginHandlersForTest().front();

      ASSERT_TRUE(handler);
      handler->CancelAuth(/*notify_others=*/true);
      auth_cancelled_waiter.Wait();
    }
  }

  EXPECT_EQ(0, browser_client_->auth_supplied_count);
  EXPECT_EQ(1, browser_client_->auth_needed_count);
  EXPECT_EQ(1, browser_client_->auth_cancelled_count);
}

// Block crossdomain image login prompting as a phishing defense.
IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTest,
                       BlockCrossdomainPromptForSubresources) {
  const char kTestPage[] = "/login/load_img_from_b.html";

  ASSERT_TRUE(embedded_test_server()->Start());

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

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page));
  }

  EXPECT_EQ(0, browser_client_->auth_needed_count);

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

    auto auth_needed_waiter = CreateAuthNeededObserver();
    browser()->OpenURL(
        OpenURLParams(test_page, Referrer(), WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false),
        /*navigation_handle_callback=*/{});
    auth_needed_waiter.Wait();
    ASSERT_EQ(1u, LoginHandler::GetAllLoginHandlersForTest().size());

    while (!LoginHandler::GetAllLoginHandlersForTest().empty()) {
      auto auth_cancelled_waiter = CreateAuthCancelledObserver();
      LoginHandler* handler =
          LoginHandler::GetAllLoginHandlersForTest().front();

      ASSERT_TRUE(handler);
      handler->CancelAuth(/*notify_others=*/true);
      auth_cancelled_waiter.Wait();
    }
  }

  EXPECT_EQ(1, browser_client_->auth_needed_count);
}

// Deep cross-domain image login prompting should be blocked, too.
IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTest,
                       BlockDeepCrossdomainPromptForSubresources) {
  const char kTestPage[] = "/iframe_login_load_img_from_b.html";

  ASSERT_TRUE(embedded_test_server()->Start());

  // b.com is iframe'd under 127.0.0.1 and includes an image. This is still
  // cross-domain.
  {
    GURL test_page = embedded_test_server()->GetURL(kTestPage);
    ASSERT_EQ("127.0.0.1", test_page.host());

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page));
  }
  EXPECT_EQ(0, browser_client_->auth_needed_count);

  // b.com iframe'd under b.com and includes an image.
  {
    GURL test_page = embedded_test_server()->GetURL(kTestPage);
    ASSERT_EQ("127.0.0.1", test_page.host());

    // Change the host from 127.0.0.1 to www.b.com so that when the
    // page tries to load from b, it will be same-origin.
    GURL::Replacements replacements;
    replacements.SetHostStr("www.b.com");
    test_page = test_page.ReplaceComponents(replacements);

    auto auth_needed_waiter = CreateAuthNeededObserver();
    browser()->OpenURL(
        OpenURLParams(test_page, Referrer(), WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false),
        /*navigation_handle_callback=*/{});
    auth_needed_waiter.Wait();
    ASSERT_EQ(1u, LoginHandler::GetAllLoginHandlersForTest().size());

    while (!LoginHandler::GetAllLoginHandlersForTest().empty()) {
      auto auth_cancelled_waiter = CreateAuthCancelledObserver();
      LoginHandler* handler =
          LoginHandler::GetAllLoginHandlersForTest().front();

      ASSERT_TRUE(handler);
      handler->CancelAuth(/*notify_others=*/true);
      auth_cancelled_waiter.Wait();
    }
  }

  EXPECT_EQ(1, browser_client_->auth_needed_count);
}

// Block same domain image resource if the top level frame is HTTPS and the
// image resource is HTTP.
// E.g. Top level: https://example.com, Image resource: http://example.com/image
IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTest,
                       BlockCrossdomainPromptForSubresourcesMixedContent) {
  ASSERT_TRUE(embedded_test_server()->Start());
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  https_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());

  GURL image_url = embedded_test_server()->GetURL("/auth-basic/index.html");
  GURL test_page = https_server.GetURL(
      std::string("/login/load_img_from_same_domain_mixed_content.html?") +
      image_url.spec());
  GURL::Replacements replacements;
  replacements.SetHostStr("a.com");
  test_page = test_page.ReplaceComponents(replacements);
  image_url = image_url.ReplaceComponents(replacements);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page));
  EXPECT_EQ(0, browser_client_->auth_needed_count);
}

// Allow crossdomain iframe login prompting despite the above.
IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTestThirdPartyCookiesUnblocked,
                       AllowCrossdomainPromptForSubframes) {
  const char kTestPage[] = "/login/load_iframe_from_b.html";

  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

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

    auto auth_needed_waiter = CreateAuthNeededObserver();
    browser()->OpenURL(
        OpenURLParams(test_page, Referrer(), WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false),
        /*navigation_handle_callback=*/{});
    auth_needed_waiter.Wait();
    ASSERT_EQ(1u, LoginHandler::GetAllLoginHandlersForTest().size());

    while (!LoginHandler::GetAllLoginHandlersForTest().empty()) {
      auto auth_cancelled_waiter = CreateAuthCancelledObserver();
      LoginHandler* handler =
          LoginHandler::GetAllLoginHandlersForTest().front();

      ASSERT_TRUE(handler);
      // When a cross origin iframe displays a login prompt, the blank
      // interstitial shouldn't be displayed and the omnibox should show the
      // main frame's url, not the iframe's.
      EXPECT_EQ(kNewHost, contents->GetVisibleURL().host());

      handler->CancelAuth(/*notify_others=*/true);
      auth_cancelled_waiter.Wait();
    }
  }

  // Should stay on the main frame's url once the prompt the iframe is closed.
  EXPECT_EQ("www.a.com", contents->GetVisibleURL().host());

  EXPECT_EQ(1, browser_client_->auth_needed_count);
}

IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTest, SupplyRedundantAuths) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Get NavigationController for tab 1.
  content::WebContents* contents_1 =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Open a new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  // Get NavigationController for tab 2.
  content::WebContents* contents_2 =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(contents_1, contents_2);

  // Open different auth urls in each tab.
  {
    auto auth_needed_waiter = CreateAuthNeededObserver();
    contents_1->OpenURL(
        OpenURLParams(embedded_test_server()->GetURL("/auth-basic/1"),
                      content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false),
        /*navigation_handle_callback=*/{});
    auth_needed_waiter.Wait();
  }

  {
    auto auth_needed_waiter = CreateAuthNeededObserver();
    contents_2->OpenURL(
        OpenURLParams(embedded_test_server()->GetURL("/auth-basic/2"),
                      content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false),
        /*navigation_handle_callback=*/{});
    auth_needed_waiter.Wait();
  }

  ASSERT_EQ(2U, LoginHandler::GetAllLoginHandlersForTest().size());

  // Supply auth in one of the tabs.
  SetAuthForAndWait(LoginHandler::GetAllLoginHandlersForTest().front());

  // Both tabs should be authenticated.
  EXPECT_EQ(2, browser_client_->auth_needed_count);
  EXPECT_EQ(2, browser_client_->auth_supplied_count);
  EXPECT_EQ(0, browser_client_->auth_cancelled_count);
}

IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTest, CancelRedundantAuths) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Get NavigationController for tab 1.
  content::WebContents* contents_1 =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Open a new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  // Get NavigationController for tab 2.
  content::WebContents* contents_2 =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(contents_1, contents_2);

  // Open different auth urls in each tab.
  {
    auto auth_needed_waiter = CreateAuthNeededObserver();
    contents_1->OpenURL(
        OpenURLParams(embedded_test_server()->GetURL("/auth-basic/1"),
                      content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false),
        /*navigation_handle_callback=*/{});
    auth_needed_waiter.Wait();
  }

  {
    auto auth_needed_waiter = CreateAuthNeededObserver();
    contents_2->OpenURL(
        OpenURLParams(embedded_test_server()->GetURL("/auth-basic/2"),
                      content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false),
        /*navigation_handle_callback=*/{});
    auth_needed_waiter.Wait();
  }

  ASSERT_EQ(2U, LoginHandler::GetAllLoginHandlersForTest().size());

  // Cancel auth in one of the tabs.
  LoginHandler* handler_1 = LoginHandler::GetAllLoginHandlersForTest().front();
  handler_1->CancelAuth(/*notify_others=*/true);

  // Both tabs should cancel auth.
  EXPECT_EQ(2, browser_client_->auth_needed_count);
  EXPECT_EQ(0, browser_client_->auth_supplied_count);
  EXPECT_EQ(2, browser_client_->auth_cancelled_count);
}

IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTest, CancelAuthWithoutNotify) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Get NavigationController for tab 1.
  content::WebContents* contents_1 =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Open a new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  // Get NavigationController for tab 2.
  content::WebContents* contents_2 =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(contents_1, contents_2);

  // Open different auth urls in each tab.
  {
    auto auth_needed_waiter = CreateAuthNeededObserver();
    contents_1->OpenURL(
        OpenURLParams(embedded_test_server()->GetURL("/auth-basic/1"),
                      content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false),
        /*navigation_handle_callback=*/{});
    auth_needed_waiter.Wait();
  }

  {
    auto auth_needed_waiter = CreateAuthNeededObserver();
    contents_2->OpenURL(
        OpenURLParams(embedded_test_server()->GetURL("/auth-basic/2"),
                      content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false),
        /*navigation_handle_callback=*/{});
    auth_needed_waiter.Wait();
  }

  ASSERT_EQ(2U, LoginHandler::GetAllLoginHandlersForTest().size());

  // Cancel auth in one of the tabs without notifying other tabs.
  LoginHandler* handler_1 = LoginHandler::GetAllLoginHandlersForTest().front();
  handler_1->CancelAuth(/*notify_others=*/false);

  // Second tab shouldn't cancel auth.
  EXPECT_EQ(2, browser_client_->auth_needed_count);
  EXPECT_EQ(0, browser_client_->auth_supplied_count);
  EXPECT_EQ(1, browser_client_->auth_cancelled_count);
}

IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTest,
                       SupplyRedundantAuthsMultiProfile) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Get NavigationController for regular tab.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Open an incognito window.
  Browser* browser_incognito = CreateIncognitoBrowser();

  // Get NavigationController for incognito tab.
  content::WebContents* contents_incognito =
      browser_incognito->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(contents, contents_incognito);

  // Open an auth url in each window.
  {
    auto auth_needed_waiter = CreateAuthNeededObserver();
    contents->OpenURL(
        OpenURLParams(embedded_test_server()->GetURL("/auth-basic/1"),
                      content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false),
        /*navigation_handle_callback=*/{});
    auth_needed_waiter.Wait();
  }
  {
    auto auth_needed_waiter = CreateAuthNeededObserver();
    contents_incognito->OpenURL(
        OpenURLParams(embedded_test_server()->GetURL("/auth-basic/2"),
                      content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false),
        /*navigation_handle_callback=*/{});
    auth_needed_waiter.Wait();
  }

  ASSERT_EQ(2U, LoginHandler::GetAllLoginHandlersForTest().size());

  // Supply auth in regular tab, it should be authenticated.
  SetAuthForAndWait(LoginHandler::GetAllLoginHandlersForTest().front());

  EXPECT_EQ(2, browser_client_->auth_needed_count);
  EXPECT_EQ(1, browser_client_->auth_supplied_count);
  EXPECT_EQ(0, browser_client_->auth_cancelled_count);
}

// If an XMLHttpRequest is made with incorrect credentials, there should be no
// login prompt; instead the 401 status should be returned to the script.
IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTest,
                       NoLoginPromptForXHRWithBadCredentials) {
  const char kXHRTestPage[] = "/login/xhr_with_credentials.html#incorrect";

  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Load a page which makes a synchronous XMLHttpRequest for an authenticated
  // resource with the wrong credentials.  There should be no login prompt.
  {
    GURL test_page = embedded_test_server()->GetURL(kXHRTestPage);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page));
  }

  std::u16string expected_title(u"status=401");

  EXPECT_EQ(expected_title, contents->GetTitle());
  EXPECT_EQ(0, browser_client_->auth_supplied_count);
  EXPECT_EQ(0, browser_client_->auth_needed_count);
  EXPECT_EQ(0, browser_client_->auth_cancelled_count);
}

// If an XMLHttpRequest is made with correct credentials, there should be no
// login prompt either.
IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTest,
                       NoLoginPromptForXHRWithGoodCredentials) {
  const char kXHRTestPage[] = "/login/xhr_with_credentials.html#secret";

  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Load a page which makes a synchronous XMLHttpRequest for an authenticated
  // resource with the correct credentials.  There should be no login prompt.
  {
    GURL test_page = embedded_test_server()->GetURL(kXHRTestPage);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page));
  }

  std::u16string expected_title(u"status=200");

  EXPECT_EQ(expected_title, contents->GetTitle());
  EXPECT_EQ(0, browser_client_->auth_supplied_count);
  EXPECT_EQ(0, browser_client_->auth_needed_count);
  EXPECT_EQ(0, browser_client_->auth_cancelled_count);
}

// If an XMLHttpRequest is made without credentials, there should be a login
// prompt.
IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTest,
                       LoginPromptForXHRWithoutCredentials) {
  const char kXHRTestPage[] = "/login/xhr_without_credentials.html";

  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Load a page which makes a synchronous XMLHttpRequest for an authenticated
  // resource without credentials.  There should be a login prompt.
  {
    GURL test_page = embedded_test_server()->GetURL(kXHRTestPage);
    auto auth_needed_waiter = CreateAuthNeededObserver();
    browser()->OpenURL(
        OpenURLParams(test_page, Referrer(), WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false),
        /*navigation_handle_callback=*/{});
    auth_needed_waiter.Wait();
  }

  ASSERT_FALSE(LoginHandler::GetAllLoginHandlersForTest().empty());
  {
    auto auth_needed_waiter = CreateAuthNeededObserver();
    auto auth_supplied_waiter = CreateAuthSuppliedObserver();
    LoginHandler* handler = LoginHandler::GetAllLoginHandlersForTest().front();

    ASSERT_TRUE(handler);
    handler->SetAuth(base::UTF8ToUTF16(bad_username_),
                     base::UTF8ToUTF16(bad_password_));
    auth_supplied_waiter.Wait();

    // The request should be retried after the incorrect password is
    // supplied.  This should result in a new AUTH_NEEDED notification
    // for the same realm.
    auth_needed_waiter.Wait();
  }

  ASSERT_EQ(1u, LoginHandler::GetAllLoginHandlersForTest().size());
  auto auth_supplied_waiter = CreateAuthSuppliedObserver();
  LoginHandler* handler = LoginHandler::GetAllLoginHandlersForTest().front();

  std::u16string username(base::UTF8ToUTF16(username_digest_));
  std::u16string password(base::UTF8ToUTF16(password_));
  handler->SetAuth(username, password);
  auth_supplied_waiter.Wait();

  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());

  std::u16string expected_title(u"status=200");

  EXPECT_EQ(expected_title, contents->GetTitle());
  EXPECT_EQ(2, browser_client_->auth_supplied_count);
  EXPECT_EQ(2, browser_client_->auth_needed_count);
  EXPECT_EQ(0, browser_client_->auth_cancelled_count);
}

// If an XMLHttpRequest is made without credentials, there should be a login
// prompt.  If it's cancelled, the script should get a 401 status.
IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTest,
                       LoginPromptForXHRWithoutCredentialsCancelled) {
  const char kXHRTestPage[] = "/login/xhr_without_credentials.html";

  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Load a page which makes a synchronous XMLHttpRequest for an authenticated
  // resource without credentials.  There should be a login prompt.
  {
    GURL test_page = embedded_test_server()->GetURL(kXHRTestPage);
    auto auth_needed_waiter = CreateAuthNeededObserver();
    browser()->OpenURL(
        OpenURLParams(test_page, Referrer(), WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false),
        /*navigation_handle_callback=*/{});
    auth_needed_waiter.Wait();
  }

  ASSERT_EQ(1u, LoginHandler::GetAllLoginHandlersForTest().size());
  auto auth_cancelled_waiter = CreateAuthCancelledObserver();
  LoginHandler* handler = LoginHandler::GetAllLoginHandlersForTest().front();

  handler->CancelAuth(/*notify_others=*/true);
  auth_cancelled_waiter.Wait();

  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());

  std::u16string expected_title(u"status=401");

  EXPECT_EQ(expected_title, contents->GetTitle());
  EXPECT_EQ(0, browser_client_->auth_supplied_count);
  EXPECT_EQ(1, browser_client_->auth_needed_count);
  EXPECT_EQ(1, browser_client_->auth_cancelled_count);
}

// Test the same scenario as LoginPromptForXHRWithoutCredentialsCancelled, but
// with the page under a service worker's control.
IN_PROC_BROWSER_TEST_P(
    LoginPromptBrowserTest,
    LoginPromptForXHRWithoutCredentialsCancelledWithServiceWorker) {
  static constexpr char kXHRTestPage[] = "/login/xhr_without_credentials.html";

  ASSERT_TRUE(embedded_https_test_server().Start());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Install a Service Worker that does nothing.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_https_test_server().GetURL(
                     "/service_worker/create_service_worker.html")));
  EXPECT_EQ("DONE", content::EvalJs(contents,
                                    "register('/service_worker/"
                                    "empty.js', '/')"));

  // Load a page which is under service worker's control and makes a synchronous
  // XMLHttpRequest for an authenticated resource without credentials. There
  // should be a login prompt.
  {
    GURL test_page = embedded_https_test_server().GetURL(kXHRTestPage);
    auto auth_needed_waiter = CreateAuthNeededObserver();
    browser()->OpenURL(
        OpenURLParams(test_page, Referrer(), WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false),
        /*navigation_handle_callback=*/{});
    auth_needed_waiter.Wait();
  }

  ASSERT_EQ(1u, LoginHandler::GetAllLoginHandlersForTest().size());
  auto auth_cancelled_waiter = CreateAuthCancelledObserver();
  LoginHandler* handler = LoginHandler::GetAllLoginHandlersForTest().front();

  handler->CancelAuth(/*notify_others=*/true);
  auth_cancelled_waiter.Wait();

  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());

  std::u16string expected_title(u"status=401");

  EXPECT_EQ(expected_title, contents->GetTitle());
  EXPECT_EQ(0, browser_client_->auth_supplied_count);
  EXPECT_EQ(1, browser_client_->auth_needed_count);
  EXPECT_EQ(1, browser_client_->auth_cancelled_count);
}

// Test that the auth cache respects NetworkIsolationKeys when splitting the
// cache based on the key is enabled.
IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTestThirdPartyCookiesUnblocked,
                       AuthCacheAcrossNetworkIsolationKeys) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_page = embedded_test_server()->GetURL(kAuthBasicPage);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  {
    auto auth_needed_waiter = CreateAuthNeededObserver();
    browser()->OpenURL(
        OpenURLParams(test_page, Referrer(), WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false),
        /*navigation_handle_callback=*/{});
    auth_needed_waiter.Wait();
  }

  ASSERT_EQ(1u, LoginHandler::GetAllLoginHandlersForTest().size());
  SetAuthForAndWait(LoginHandler::GetAllLoginHandlersForTest().front());
  ExpectSuccessfulBasicAuthTitle(contents);
  EXPECT_EQ(1, browser_client_->auth_needed_count);

  base::RunLoop run_loop;
  browser()
      ->profile()
      ->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->ClearHttpCache(base::Time(), base::Time(), nullptr,
                       run_loop.QuitClosure());
  run_loop.Run();

  // Navigate to a URL on a different origin that iframes the URL with the
  // challenge.
  GURL cross_origin_page = embedded_test_server()->GetURL(
      "localhost", "/iframe?" + test_page.spec());
  if (GetParam() == SplitAuthCacheByNetworkIsolationKey::kFalse) {
    // When allowing credentials to be used across NetworkIsolationKeys, the
    // auth credentials should be reused and there should be no new auth dialog.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), cross_origin_page));
    EXPECT_EQ(0u, LoginHandler::GetAllLoginHandlersForTest().size());
    EXPECT_EQ(1, browser_client_->auth_needed_count);
  } else {
    // When not allowing credentials to be used across NetworkIsolationKeys,
    // there should be another auth challenge.
    content::TestNavigationObserver navigation_observer(contents);
    auto auth_needed_waiter = CreateAuthNeededObserver();
    browser()->OpenURL(OpenURLParams(cross_origin_page, Referrer(),
                                     WindowOpenDisposition::CURRENT_TAB,
                                     ui::PAGE_TRANSITION_TYPED, false),
                       /*navigation_handle_callback=*/{});
    auth_needed_waiter.Wait();
    ASSERT_EQ(1u, LoginHandler::GetAllLoginHandlersForTest().size());
    SetAuthForAndWait(LoginHandler::GetAllLoginHandlersForTest().front());
    navigation_observer.Wait();
    EXPECT_EQ(2, browser_client_->auth_needed_count);
  }

  content::RenderFrameHost* child_frame = ChildFrameAt(contents, 0);
  ASSERT_TRUE(child_frame);
  ASSERT_EQ(test_page, child_frame->GetLastCommittedURL());

  // Make sure the iframe is displaying the base64-encoded credentials that
  // should have been set, which the EmbeddedTestServer echos back in response
  // bodies when /basic-auth is requested.
  EXPECT_EQ(true, content::EvalJs(child_frame,
                                  "document.documentElement.innerText.search("
                                  "'YmFzaWN1c2VyOnNlY3JldA==') >= 0"));
}

IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTestThirdPartyCookiesUnblocked,
                       GloballyScopeHTTPAuthCacheEnabled) {
  ASSERT_TRUE(embedded_test_server()->Start());

  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kGloballyScopeHTTPAuthCacheEnabled, true);
  // This is not technically necessary, since the SetAuthFor() call below uses
  // the same pipe that the pref change uses, making sure the change is applied
  // before the network process receives credentials, but seems safest to flush
  // the NetworkContext pipe explicitly.
  browser()
      ->profile()
      ->GetDefaultStoragePartition()
      ->FlushNetworkInterfaceForTesting();

  GURL test_page = embedded_test_server()->GetURL(kAuthBasicPage);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  auto auth_needed_waiter = CreateAuthNeededObserver();
  browser()->OpenURL(
      OpenURLParams(test_page, Referrer(), WindowOpenDisposition::CURRENT_TAB,
                    ui::PAGE_TRANSITION_TYPED, false),
      /*navigation_handle_callback=*/{});
  auth_needed_waiter.Wait();

  ASSERT_EQ(1u, LoginHandler::GetAllLoginHandlersForTest().size());
  SetAuthForAndWait(LoginHandler::GetAllLoginHandlersForTest().front());
  ExpectSuccessfulBasicAuthTitle(contents);

  EXPECT_EQ(1, browser_client_->auth_needed_count);

  base::RunLoop run_loop;
  browser()
      ->profile()
      ->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->ClearHttpCache(base::Time(), base::Time(), nullptr,
                       run_loop.QuitClosure());
  run_loop.Run();

  // Navigate to a URL on a different origin that iframes the URL with the
  // challenge.
  GURL cross_origin_page = embedded_test_server()->GetURL(
      "localhost", "/iframe?" + test_page.spec());

  // When allowing credentials to be used across NetworkIsolationKeys, the
  // auth credentials should be reused and there should be no new auth dialog.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), cross_origin_page));
  EXPECT_EQ(0u, LoginHandler::GetAllLoginHandlersForTest().size());
  EXPECT_EQ(1, browser_client_->auth_needed_count);

  content::RenderFrameHost* child_frame = ChildFrameAt(contents, 0);
  ASSERT_TRUE(child_frame);
  ASSERT_EQ(test_page, child_frame->GetLastCommittedURL());

  // Make sure the iframe is displaying the base64-encoded credentials that
  // should have been set, which the EmbeddedTestServer echos back in response
  // bodies when /basic-auth is requested.
  EXPECT_EQ(true, content::EvalJs(child_frame,
                                  "document.documentElement.innerText.search("
                                  "'YmFzaWN1c2VyOnNlY3JldA==') >= 0"));
}

// If a cross origin direct navigation triggers a login prompt, the login
// interstitial should be shown.
IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTest,
                       ShowCorrectUrlForCrossOriginMainFrameRequests) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL test_page = embedded_test_server()->GetURL(kAuthBasicPage);
  ASSERT_EQ("127.0.0.1", test_page.host());
  std::string auth_host("127.0.0.1");
  TestCrossOriginPrompt(browser(), test_page, auth_host, true);
}

// Same as ShowCorrectUrlForCrossOriginMainFrameRequests, but happening in a
// popup window.
IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTest,
                       ShowCorrectUrlForCrossOriginMainFrameRequests_Popup) {
  ASSERT_TRUE(embedded_test_server()->Start());

  Browser* popup = CreateBrowserForPopup(browser()->profile());
  const GURL test_page = embedded_test_server()->GetURL(kAuthBasicPage);
  ASSERT_EQ("127.0.0.1", test_page.host());
  const std::string auth_host("127.0.0.1");
  TestCrossOriginPrompt(popup, test_page, auth_host, true);
}

// If a cross origin redirect triggers a login prompt, the destination URL
// should be shown in the omnibox when the auth dialog is displayed.
IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTest,
                       ShowCorrectUrlForCrossOriginMainFrameRedirects) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const char kTestPage[] = "/login/cross_origin.html";
  GURL test_page = embedded_test_server()->GetURL(kTestPage);
  ASSERT_EQ("127.0.0.1", test_page.host());
  std::string auth_host("www.a.com");
  TestCrossOriginPrompt(browser(), test_page, auth_host, true);
}

// Same as above, but instead of cancelling the prompt for www.a.com at the end,
// the page redirects to another page (www.b.com) that triggers an auth dialog.
// This should cancel the login interstitial for the first page (www.a.com),
// create a blank interstitial for second page (www.b.com) and show its URL in
// the omnibox.

// Fails occasionally on Mac. http://crbug.com/852703
#if BUILDFLAG(IS_MAC)
#define MAYBE_CancelLoginInterstitialOnRedirect \
  DISABLED_CancelLoginInterstitialOnRedirect
#else
#define MAYBE_CancelLoginInterstitialOnRedirect \
  CancelLoginInterstitialOnRedirect
#endif
IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTest,
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

  // Load the test page. It should end up on www.a.com with the auth dialog
  // open.
  TestCrossOriginPrompt(browser(), test_page, "www.a.com", false);
  ASSERT_EQ(1u, LoginHandler::GetAllLoginHandlersForTest().size());

  // While the auth dialog is open for www.a.com, redirect to www.b.com which
  // also triggers an auth dialog. This should cancel the auth dialog for
  // www.a.com and end up displaying an auth interstitial and the URL for
  // www.b.com.
  auto auth_cancelled_waiter = CreateAuthCancelledObserver();
  auto auth_needed_waiter = CreateAuthNeededObserver();
  {
    EXPECT_TRUE(content::ExecJs(
        contents, std::string("document.location='") + page2.spec() + "';"));
    auth_cancelled_waiter.Wait();
    // Wait for the auth dialog and the interstitial for www.b.com.
    auth_needed_waiter.Wait();
    ASSERT_EQ(1u, LoginHandler::GetAllLoginHandlersForTest().size());
    content::WaitForLoadStop(contents);
  }

  EXPECT_EQ("www.b.com", contents->GetVisibleURL().host());

  // Cancel auth dialog for www.b.com.
  LoginHandler* handler = LoginHandler::GetAllLoginHandlersForTest().front();
  handler->CancelAuth(/*notify_others=*/true);
  EXPECT_EQ("www.b.com", contents->GetVisibleURL().host());
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
// is any other interstitial being displayed. With committed SSL interstitials,
// the navigation is actually cross domain since the interstitial is actually
// a committed navigation, but we still expect the same behavior.
IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTest,
                       ShouldReplaceExistingInterstitialWhenNavigated) {
  ASSERT_TRUE(embedded_test_server()->Start());
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  ASSERT_TRUE(https_server.Start());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL auth_url = embedded_test_server()->GetURL(kAuthBasicPage);
  GURL broken_ssl_page = https_server.GetURL("/");

  // Navigate to an auth url and wait for the login prompt.
  {
    auto auth_needed_waiter = CreateAuthNeededObserver();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), auth_url));
    ASSERT_EQ("127.0.0.1", contents->GetLastCommittedURL().host());
    ASSERT_TRUE(contents->GetLastCommittedURL().SchemeIs("http"));
    auth_needed_waiter.Wait();
    ASSERT_EQ(1u, LoginHandler::GetAllLoginHandlersForTest().size());
    // Cancel the auth prompt, which triggers a reload.
    LoginHandler* handler = LoginHandler::GetAllLoginHandlersForTest().front();
    content::TestNavigationObserver reload_observer(contents);
    handler->CancelAuth(/*notify_others=*/true);
    reload_observer.Wait();
    EXPECT_EQ("127.0.0.1", contents->GetVisibleURL().host());
    EXPECT_EQ(auth_url, contents->GetLastCommittedURL());
  }

  // Navigate to a broken SSL page. This is a cross origin navigation since
  // schemes don't match (http vs https).
  {
    ASSERT_EQ("127.0.0.1", broken_ssl_page.host());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), broken_ssl_page));
    ASSERT_EQ("127.0.0.1", contents->GetLastCommittedURL().host());
    ASSERT_TRUE(contents->GetLastCommittedURL().SchemeIs("https"));
    ASSERT_TRUE(WaitForRenderFrameReady(contents->GetPrimaryMainFrame()));
  }

  // An overrideable SSL interstitial is now being displayed. Navigate to the
  // auth URL again. This is again a cross origin navigation, but last committed
  // URL is the same as the auth URL (since SSL navigation never committed).
  // Should still replace SSL interstitial with an auth interstitial even though
  // last committed URL and the new URL is the same. With committed SSL
  // interstitials enabled we still check for the behavior, but the
  // SSL interstitial will be a committed navigation so it will be handled as a
  // cross origin navigation.
  {
    auto auth_needed_waiter = CreateAuthNeededObserver();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), auth_url));
    ASSERT_EQ("127.0.0.1", contents->GetLastCommittedURL().host());
    ASSERT_TRUE(contents->GetLastCommittedURL().SchemeIs("http"));

    auth_needed_waiter.Wait();
    ASSERT_EQ(1u, LoginHandler::GetAllLoginHandlersForTest().size());
    // The login prompt is displayed above an empty page.
    EXPECT_EQ("<head></head><body></body>",
              content::EvalJs(contents, "document.documentElement.innerHTML"));
  }
}

// Test that the login interstitial isn't proceeding itself or any other
// interstitial. If this test becomes flaky, it's likely that the logic that
// prevents the tested scenario from happening got broken, rather than the test
// itself.
IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTestThirdPartyCookiesUnblocked,
                       ShouldNotProceedExistingInterstitial) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  ASSERT_TRUE(https_server.Start());

  const char* kTestPage = "/login/load_iframe_from_b.html";

  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Load a page that has a cross-domain iframe authentication. This should
  // trigger a login prompt but no login interstitial.
  GURL test_page = embedded_test_server()->GetURL(kTestPage);
  GURL broken_ssl_page = https_server.GetURL("/");
  ASSERT_EQ("127.0.0.1", test_page.host());
  auto auth_needed_waiter = CreateAuthNeededObserver();
  browser()->OpenURL(
      OpenURLParams(test_page, Referrer(), WindowOpenDisposition::CURRENT_TAB,
                    ui::PAGE_TRANSITION_TYPED, false),
      /*navigation_handle_callback=*/{});
  auth_needed_waiter.Wait();
  ASSERT_EQ(1u, LoginHandler::GetAllLoginHandlersForTest().size());
  security_interstitials::SecurityInterstitialPage* interstitial =
      GetSecurityInterstitial(contents);
  EXPECT_FALSE(interstitial);

  // Redirect to a broken SSL page. This redirect should not accidentally
  // proceed through the SSL interstitial.
  content::TestNavigationObserver ssl_observer(contents);
  EXPECT_TRUE(content::ExecJs(
      browser()->tab_strip_model()->GetActiveWebContents(),
      std::string("window.location = '") + broken_ssl_page.spec() + "'"));
  ssl_observer.Wait();

  interstitial = GetSecurityInterstitial(contents);

  EXPECT_TRUE(interstitial);
  EXPECT_EQ(SSLBlockingPage::kTypeForTesting,
            interstitial->GetTypeForTesting());
}

// Test where Basic HTTP authentication is disabled.
IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTest, PRE_TestBasicAuthDisabled) {
  // Disable all auth schemes. The modified list isn't respected until the
  // browser is restarted, however.
  g_browser_process->local_state()->SetString(prefs::kAuthSchemes, "");
}

IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTest, TestBasicAuthDisabled) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_page = embedded_test_server()->GetURL(kAuthBasicPage);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // If the network service crashes, basic auth should still be disabled.
  for (bool crash_network_service : {false, true}) {
    // Crash the network service if it is enabled.
    if (crash_network_service && content::IsOutOfProcessNetworkService()) {
      SimulateNetworkServiceCrash();
      // Flush the network interface to make sure it notices the crash.
      browser()
          ->profile()
          ->GetDefaultStoragePartition()
          ->FlushNetworkInterfaceForTesting();
    }

    browser()->OpenURL(
        OpenURLParams(test_page, Referrer(), WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false),
        /*navigation_handle_callback=*/{});
    EXPECT_EQ(0, browser_client_->auth_supplied_count);

    const std::u16string kExpectedTitle =
        u"Denied: Missing Authorization Header";
    content::TitleWatcher title_watcher(contents, kExpectedTitle);
    EXPECT_EQ(kExpectedTitle, title_watcher.WaitAndGetTitle());
  }
}

// Tests that when HTTP Auth committed interstitials are enabled, a cross-origin
// main-frame auth challenge cancels the auth request.
IN_PROC_BROWSER_TEST_P(
    LoginPromptBrowserTest,
    TestAuthChallengeCancelsNavigationWithCommittedInterstitials) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL test_page = embedded_test_server()->GetURL(kAuthBasicPage);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page));

  // The login prompt should display above an empty page.
  EXPECT_EQ("<head></head><body></body>",
            content::EvalJs(contents, "document.documentElement.innerHTML"));
  EXPECT_EQ(0, browser_client_->auth_cancelled_count);
}

// Tests that when HTTP Auth committed interstitials are enabled, the login
// prompt is shown on top of a committed error page when there is a cross-origin
// main-frame auth challenge.
IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTest,
                       PromptWithCommittedInterstitials) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto auth_needed_waiter = CreateAuthNeededObserver();

  GURL test_page = embedded_test_server()->GetURL(kAuthBasicPage);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page));

  // Test that the login prompt displays above an empty page.
  EXPECT_EQ("<head></head><body></body>",
            content::EvalJs(contents, "document.documentElement.innerHTML"));

  auth_needed_waiter.Wait();
  ASSERT_EQ(1u, LoginHandler::GetAllLoginHandlersForTest().size());

  // Test that credentials are handled correctly.
  SetAuthForAndWait(LoginHandler::GetAllLoginHandlersForTest().front());
  ExpectSuccessfulBasicAuthTitle(contents);
}

// Tests that the repost dialog is not shown when credentials are entered for a
// POST navigation. Regression test for https://crbug.com/1062317.
IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTest, NoRepostDialogAfterCredentials) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto auth_needed_waiter = CreateAuthNeededObserver();

  // Navigate to a blank page and inject a form to trigger a POST navigation
  // that requests credentials.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/login/form.html")));
  ASSERT_TRUE(
      content::ExecJs(contents, "document.getElementById('submit').click()"));

  auth_needed_waiter.Wait();
  ASSERT_EQ(1u, LoginHandler::GetAllLoginHandlersForTest().size());

  // Enter credentials and test that the page loads. If the repost dialog is
  // shown, the test will hang while waiting for input.
  SetAuthForAndWait(LoginHandler::GetAllLoginHandlersForTest().front());
  ExpectSuccessfulBasicAuthTitle(contents);
}

// Tests that when HTTP Auth committed interstitials are enabled, showing a
// login prompt in a new window opened from window.open() does not
// crash. Regression test for https://crbug.com/1005096.
IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTest, PromptWithOnlyInitialEntry) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  // Open a new window via JavaScript and navigate it to a page that delivers an
  // auth prompt.
  GURL test_page = embedded_test_server()->GetURL(kAuthBasicPage);
  ASSERT_NE(false, content::EvalJs(contents, "w = window.open('/nocontent');"));
  content::WebContents* opened_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  NavigationController* opened_controller = &opened_contents->GetController();
  ASSERT_TRUE(opened_controller->GetVisibleEntry()->IsInitialEntry());
  auto auth_needed_waiter = CreateAuthNeededObserver();
  ASSERT_NE(false, content::EvalJs(contents, "w.location.href = '" +
                                                 test_page.spec() + "';"));

  // Test that the login prompt displays above an empty page.
  EXPECT_EQ(
      "<head></head><body></body>",
      content::EvalJs(opened_contents, "document.documentElement.innerHTML"));

  auth_needed_waiter.Wait();
  ASSERT_EQ(1u, LoginHandler::GetAllLoginHandlersForTest().size());

  // Test that credentials are handled correctly.
  SetAuthForAndWait(LoginHandler::GetAllLoginHandlersForTest().front());
  ExpectSuccessfulBasicAuthTitle(opened_contents);
}

// Tests that when HTTP Auth committed interstitials are enabled, a prompt
// triggered by a subframe can be cancelled.
IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTestThirdPartyCookiesUnblocked,
                       PromptFromSubframe) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // Via JavaScript, create an iframe that delivers an auth prompt.
  GURL test_page = embedded_test_server()->GetURL(kAuthBasicPage);
  content::TestNavigationObserver subframe_observer(contents);
  auto auth_needed_waiter = CreateAuthNeededObserver();
  ASSERT_NE(
      false,
      content::EvalJs(
          contents, "var i = document.createElement('iframe'); i.src = '" +
                        test_page.spec() + "'; document.body.appendChild(i);"));
  auth_needed_waiter.Wait();
  ASSERT_EQ(1u, LoginHandler::GetAllLoginHandlersForTest().size());

  // Cancel the prompt and check that another prompt is not shown.
  auto auth_cancelled_waiter = CreateAuthCancelledObserver();
  LoginHandler* handler = LoginHandler::GetAllLoginHandlersForTest().front();
  handler->CancelAuth(/*notify_others=*/true);
  auth_cancelled_waiter.Wait();
  subframe_observer.Wait();
  EXPECT_EQ(0u, LoginHandler::GetAllLoginHandlersForTest().size());
}

namespace {

// A request handler that returns a 401 Unauthorized response on the
// /unauthorized path.
std::unique_ptr<net::test_server::HttpResponse> HandleUnauthorized(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != "/unauthorized") {
    return nullptr;
  }

  std::unique_ptr<net::test_server::BasicHttpResponse> response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_UNAUTHORIZED);
  response->set_content("<html><body>Unauthorized</body></html>");
  return response;
}

}  // namespace

// Tests that 401 responses are not cancelled and replaced with a blank page
// when incorrect credentials were supplied in the request. See
// https://crbug.com/1047742.
IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTest,
                       ResponseNotCancelledWithIncorrectCredentials) {
  // Register a custom handler that returns a 401 Unauthorized response
  // regardless of what credentials were supplied in the request.
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&HandleUnauthorized));
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_page = embedded_test_server()->GetURL(kAuthBasicPage);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to a page that prompts basic auth and fill in correct
  // credentials. A subsequent navigation handled by HandleUnauthorized() will
  // send the credentials cached from the navigation to |test_page|, but return
  // a 401 Unauthorized response.
  auto auth_needed_waiter = CreateAuthNeededObserver();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page));
  auth_needed_waiter.Wait();
  SetAuthForAndWait(LoginHandler::GetAllLoginHandlersForTest().front());
  ExpectSuccessfulBasicAuthTitle(web_contents);

  // Now navigate to a page handled by HandleUnauthorized(), for which the
  // cached credentials are incorrect.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/unauthorized")));
  // Test that the 401 response body is rendered, instead of the navigation
  // being cancelled and a blank error page committing.
  EXPECT_EQ(false,
            content::EvalJs(
                web_contents,
                "document.body.innerHTML.indexOf('Unauthorized') === -1"));
}

class LoginProxyBrowserTest : public ProxyBrowserTest,
                              public LoginPromptBrowserTestHelper {
 public:
  void SetUpOnMainThread() override {
    SetUpLoginFakes();
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TestProxyAuth(Browser* browser, const GURL& test_page) {
    bool https = test_page.SchemeIs(url::kHttpsScheme);

    content::WebContents* contents =
        browser->tab_strip_model()->GetActiveWebContents();

    {
      auto auth_needed_waiter = CreateAuthNeededObserver();
      ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, test_page));
      auth_needed_waiter.Wait();
    }

    // On HTTPS pages, no error page content should be rendered to avoid origin
    // confusion issues.
    if (https) {
      EXPECT_FALSE(contents->IsLoading());
      EXPECT_EQ(
          "<head></head><body></body>",
          content::EvalJs(contents, "document.documentElement.innerHTML"));
    }

    // The URL should be hidden to avoid origin confusion issues.
    EXPECT_TRUE(browser->location_bar_model()->GetFormattedFullURL().empty());

    // Cancel the prompt, which triggers a reload to read the error page content
    // from the server. On HTTPS pages, the error page content still shouldn't
    // be shown.
    {
      auto auth_cancelled_waiter = CreateAuthCancelledObserver();
      LoginHandler* handler =
          LoginHandler::GetAllLoginHandlersForTest().front();
      content::TestNavigationObserver reload_observer(contents);
      handler->CancelAuth(/*notify_others=*/true);
      auth_cancelled_waiter.Wait();
      reload_observer.Wait();
      if (https) {
        EXPECT_EQ(
            "<head></head><body></body>",
            content::EvalJs(contents, "document.documentElement.innerHTML"));
      }

      EXPECT_FALSE(
          browser->location_bar_model()->GetFormattedFullURL().empty());
    }

    // Reload; this time, supply credentials and check that the page loads.
    {
      auto auth_needed_waiter = CreateAuthNeededObserver();
      browser->OpenURL(OpenURLParams(test_page, Referrer(),
                                     WindowOpenDisposition::CURRENT_TAB,
                                     ui::PAGE_TRANSITION_TYPED, false),
                       /*navigation_handle_callback=*/{});
      auth_needed_waiter.Wait();
      EXPECT_TRUE(browser->location_bar_model()->GetFormattedFullURL().empty());
    }

    auto auth_supplied_waiter = CreateAuthSuppliedObserver();
    LoginHandler* handler = LoginHandler::GetAllLoginHandlersForTest().front();
    handler->SetAuth(u"foo", u"bar");
    auth_supplied_waiter.Wait();

    std::u16string expected_title = u"OK";
    content::TitleWatcher title_watcher(contents, expected_title);
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
    EXPECT_FALSE(browser->location_bar_model()->GetFormattedFullURL().empty());
  }
};

// Tests that basic proxy auth works as expected, for HTTPS pages.
#if BUILDFLAG(IS_MAC)
// TODO(crbug.com/40645658): Re-enable this test.
#define MAYBE_ProxyAuthHTTPS DISABLED_ProxyAuthHTTPS
#else
#define MAYBE_ProxyAuthHTTPS ProxyAuthHTTPS
#endif
IN_PROC_BROWSER_TEST_F(LoginProxyBrowserTest, MAYBE_ProxyAuthHTTPS) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());
  ASSERT_NO_FATAL_FAILURE(
      TestProxyAuth(browser(), https_server.GetURL("/simple.html")));
}

// Tests that basic proxy auth works as expected, for HTTP pages.
IN_PROC_BROWSER_TEST_F(LoginProxyBrowserTest, ProxyAuthHTTP) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_NO_FATAL_FAILURE(
      TestProxyAuth(browser(), embedded_test_server()->GetURL("/simple.html")));
}

class LoginPromptExtensionBrowserTest
    : public extensions::ExtensionBrowserTest,
      public LoginPromptBrowserTestHelper,
      public testing::WithParamInterface<SplitAuthCacheByNetworkIsolationKey> {
 public:
  LoginPromptExtensionBrowserTest() {
    scoped_feature_list_.InitWithFeatureStates(
        {{network::features::kSplitAuthCacheByNetworkIsolationKey,
          (GetParam() == SplitAuthCacheByNetworkIsolationKey::kTrue)}});
    }

    void SetUpOnMainThread() override {
      SetUpLoginFakes();
      extensions::ExtensionBrowserTest::SetUpOnMainThread();
    }

  ~LoginPromptExtensionBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    LoginPromptExtensionBrowserTest,
    ::testing::Values(SplitAuthCacheByNetworkIsolationKey::kFalse,
                      SplitAuthCacheByNetworkIsolationKey::kTrue));

// Tests that with committed interstitials, extensions are notified once per
// request when auth is required. Regression test for https://crbug.com/1034468.
IN_PROC_BROWSER_TEST_P(LoginPromptExtensionBrowserTest,
                       OnAuthRequiredNotifiedOnce) {
  const char kSlowResponse[] = "/slow-response";

  // Once the request has been made, this will be set with a closure to finish
  // the slow response.
  base::OnceClosure finish_slow_response;

  embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url != kSlowResponse)
          return nullptr;
        auto response = std::make_unique<SlowAuthResponse>(
            base::BindLambdaForTesting([&](base::OnceClosure start_response,
                                           base::OnceClosure finish_response) {
              // The response is started immediately, but we delay finishing it.
              std::move(start_response).Run();
              finish_slow_response = std::move(finish_response);
            }));
        return response;
      }));
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load an extension that logs to the console each time onAuthRequired is
  // called. We attach a console observer so that we can verify that the
  // extension only logs once per request.
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("log_auth_required"));
  ASSERT_TRUE(extension);
  content::WebContentsConsoleObserver console_observer(
      extensions::ProcessManager::Get(profile())
          ->GetBackgroundHostForExtension(extension->id())
          ->host_contents());

  // Navigate to a page that prompts for basic auth and then hangs.
  auto auth_needed_waiter = CreateAuthNeededObserver();
  GURL test_page = embedded_test_server()->GetURL(kSlowResponse);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page));

  ASSERT_TRUE(console_observer.Wait());
  ASSERT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ(u"onAuthRequired " + base::ASCIIToUTF16(test_page.spec()),
            console_observer.messages()[0].message);

  // End the response that prompted for basic auth.
  std::move(finish_slow_response).Run();

  // If https://crbug.com/1034468 regresses, the test may hang here. In that
  // bug, extensions were getting notified of each auth request twice, and the
  // extension must handle the auth request both times before LoginHandler
  // proceeds to show the login prompt. Usually, the request is fully destroyed
  // before the second extension dispatch, so the second extension dispatch is a
  // no-op. But when there is a delay between the OnAuthRequired notification
  // and the response body being read (as provided by SlowAuthResponse), the
  // WebRequestAPI is notified that the request is destroyed between the second
  // dispatch to an extension and when the extension replies. When this happens,
  // the LoginHandler is never notified that it can continue to show the login
  // prompt, so the auth needed notification that we are waiting for <will never
  // come. The fix to this bug is to ensure that extensions are notified of each
  // auth request only once; this test verifies that condition by checking that
  // the auth needed notification comes as expected and that the test extension
  // only logs once for onAuthRequired.
  auth_needed_waiter.Wait();
  // No second console message should have been logged, because extensions
  // should only be notified of the auth request once.
  EXPECT_EQ(1u, console_observer.messages().size());

  // It's possible that a second message was in fact logged, but the observer
  // hasn't heard about it yet. Navigate to a different URL and wait for the
  // corresponding console message, to "flush" any possible second message from
  // the current page load.
  auto second_auth_needed_waiter = CreateAuthNeededObserver();
  GURL second_test_page = embedded_test_server()->GetURL("/auth-basic");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), second_test_page));
  second_auth_needed_waiter.Wait();
  ASSERT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ(u"onAuthRequired " + base::ASCIIToUTF16(second_test_page.spec()),
            console_observer.messages()[1].message);
}

// Tests that extensions can cancel authentication requests to suppress a
// prompt. Regression test for https://crbug.com/1075442.
IN_PROC_BROWSER_TEST_P(LoginPromptExtensionBrowserTest, OnAuthRequiredCancels) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load an extension that cancels each auth request.
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("cancel_auth_required"));
  ASSERT_TRUE(extension);

  // Navigate to a page that prompts for basic auth and test that the 401
  // response body is rendered.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL test_page = embedded_test_server()->GetURL(kAuthBasicPage);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page));

  std::u16string expected_title(u"Denied: Missing Authorization Header");
  EXPECT_EQ(expected_title, contents->GetTitle());
}

// Tests that login prompts are shown for main resource requests that are
// intercepted by service workers. Regression test for
// https://crbug.com/1055253.
IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTest, BasicAuthWithServiceWorker) {
  net::test_server::EmbeddedTestServer https_server(
      net::test_server::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Install a Service Worker that responds to fetch events by fetch()ing the
  // requested resource.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_server.GetURL("/service_worker/create_service_worker.html")));
  EXPECT_EQ("DONE",
            content::EvalJs(web_contents,
                            "register('/service_worker/"
                            "fetch_event_respond_with_fetch.js', '/')"));

  // Now navigate to a page that requests basic auth.
  {
    auto auth_needed_waiter = CreateAuthNeededObserver();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), https_server.GetURL(kAuthBasicPage)));
    auth_needed_waiter.Wait();
    EXPECT_FALSE(LoginHandler::GetAllLoginHandlersForTest().empty());

    // Cancel the auth prompt and check that the 401 response is displayed after
    // a reload.
    LoginHandler* handler = LoginHandler::GetAllLoginHandlersForTest().front();
    content::TestNavigationObserver reload_observer(web_contents);
    handler->CancelAuth(/*notify_others=*/true);
    reload_observer.Wait();
    const std::u16string kExpectedTitle =
        u"Denied: Missing Authorization Header";
    EXPECT_EQ(kExpectedTitle, web_contents->GetTitle());
  }

  // Reload and provide correct credentials this time.
  {
    auto auth_needed_waiter = CreateAuthNeededObserver();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), https_server.GetURL(kAuthBasicPage)));
    auth_needed_waiter.Wait();
    EXPECT_FALSE(LoginHandler::GetAllLoginHandlersForTest().empty());
    SetAuthForAndWait(LoginHandler::GetAllLoginHandlersForTest().front());
    ExpectSuccessfulBasicAuthTitle(web_contents);
  }
}

namespace {

const char kWorkerHttpBasicAuthPath[] =
    "/service_worker/http_basic_auth?intercept";

// Serves a Basic Auth challenge.
std::unique_ptr<net::test_server::HttpResponse> HandleHttpAuthRequest(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != kWorkerHttpBasicAuthPath)
    return nullptr;

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_UNAUTHORIZED);
  http_response->AddCustomHeader("WWW-Authenticate",
                                 "Basic realm=\"test realm\"");
  return http_response;
}

}  // namespace

// Tests that crash doesn't happen, when the service worker calls fetch() for a
// subresource and the page is destroyed before OnAuthRequired() is called. This
// is a regression test for https://crbug.com/1320420.
IN_PROC_BROWSER_TEST_P(LoginPromptBrowserTest,
                       BasicAuthWithServiceWorkerForFetchSubResource) {
  net::test_server::EmbeddedTestServer https_server(
      net::test_server::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  https_server.RegisterRequestHandler(
      base::BindRepeating(&HandleHttpAuthRequest));
  auto test_server_handle = https_server.StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle);

  // Open a new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  // There are two tabs.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Install a Service Worker that responds to fetch events by fetch()ing the
  // requested resource.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_server.GetURL("/service_worker/create_service_worker.html")));
  EXPECT_EQ("DONE",
            content::EvalJs(web_contents,
                            "register('/service_worker/"
                            "fetch_event_respond_with_fetch.js', '/')"));

  // Now navigate to a simple page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server.GetURL("/simple.html")));

  // Write a JS to fetch a sub resource.
  const std::string register_script = base::StringPrintf(
      R"(
        function try_fetch_status(url) {
          return fetch(url).then(
            response => {
              return response.status;
            },
            err => {
              return err.name;
            }
          );
        }
        try_fetch_status('%s');
  )",
      https_server.GetURL(kWorkerHttpBasicAuthPath).spec().c_str());

  // Run JS asynchronously.
  std::ignore = content::EvalJs(
      web_contents, register_script,
      content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES);

  // Close the current active tab and it should not crash. By the JS above,
  // OnAuthRequired() in StoragePartitionImpl is called even after the web
  // contents is destroyed but it passes the empty credential info and
  // triggers CancelAuth().
  browser()->tab_strip_model()->CloseWebContentsAt(1,
                                                   TabCloseTypes::CLOSE_NONE);

  // It has one tab left.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Ensure that a new navigation works in the current web contents.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server.GetURL("/simple.html")));
}

class LoginPromptPrerenderBrowserTest : public LoginPromptBrowserTest {
 public:
  LoginPromptPrerenderBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &LoginPromptPrerenderBrowserTest::GetWebContents,
            base::Unretained(this))) {}
  ~LoginPromptPrerenderBrowserTest() override = default;
  LoginPromptPrerenderBrowserTest(const LoginPromptPrerenderBrowserTest&) =
      delete;
  LoginPromptPrerenderBrowserTest& operator=(
      const LoginPromptPrerenderBrowserTest&) = delete;

  void SetUp() override { LoginPromptBrowserTest::SetUp(); }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    LoginPromptBrowserTest::SetUpOnMainThread();
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    LoginPromptPrerenderBrowserTest,
    ::testing::Values(SplitAuthCacheByNetworkIsolationKey::kFalse,
                      SplitAuthCacheByNetworkIsolationKey::kTrue));

IN_PROC_BROWSER_TEST_P(LoginPromptPrerenderBrowserTest, CancelOnAuthRequested) {
  base::HistogramTester histogram_tester;

  // Navigate to an initial page.
  const GURL kInitialUrl = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kInitialUrl));

  // Start prerendering `kPrerenderingUrl`.
  const GURL kPrerenderingUrl = embedded_test_server()->GetURL(kAuthBasicPage);
  prerender_helper().AddPrerenderAsync(kPrerenderingUrl);
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetWebContents());
  registry_observer.WaitForTrigger(kPrerenderingUrl);
  content::FrameTreeNodeId host_id =
      prerender_helper().GetHostForUrl(kPrerenderingUrl);
  content::test::PrerenderHostObserver host_observer(*GetWebContents(),
                                                     host_id);

  // The prerender should be destroyed.
  host_observer.WaitForDestroyed();
  EXPECT_TRUE(prerender_helper().GetHostForUrl(kPrerenderingUrl).is_null());

  // No authentication request has been prompted to the user.
  EXPECT_EQ(0, browser_client_->auth_needed_count);
}

IN_PROC_BROWSER_TEST_P(LoginPromptPrerenderBrowserTest,
                       CancelOnAuthRequestedSubFrame) {
  base::HistogramTester histogram_tester;

  // Navigate to an initial page.
  const GURL kInitialUrl = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kInitialUrl));

  // Start prerendering `kPrerenderingUrl`.
  const GURL kPrerenderingUrl = embedded_test_server()->GetURL("/title1.html");
  content::FrameTreeNodeId host_id =
      prerender_helper().AddPrerender(kPrerenderingUrl);
  content::test::PrerenderHostObserver host_observer(*GetWebContents(),
                                                     host_id);
  prerender_helper().WaitForPrerenderLoadCompletion(kPrerenderingUrl);

  // Fetch a subframe that requires authentication.
  const GURL kAuthIFrameUrl = embedded_test_server()->GetURL(kAuthBasicPage);
  content::RenderFrameHost* prerender_rfh =
      prerender_helper().GetPrerenderedMainFrameHost(host_id);
  std::ignore =
      ExecJs(prerender_rfh,
             "var i = document.createElement('iframe'); i.src = '" +
                 kAuthIFrameUrl.spec() + "'; document.body.appendChild(i);");

  // The prerender should be destroyed.
  host_observer.WaitForDestroyed();
  EXPECT_TRUE(prerender_helper().GetHostForUrl(kPrerenderingUrl).is_null());

  // No authentication request has been prompted to the user.
  EXPECT_EQ(0, browser_client_->auth_needed_count);
}

IN_PROC_BROWSER_TEST_P(LoginPromptPrerenderBrowserTest,
                       CancelOnAuthRequestedSubResource) {
  base::HistogramTester histogram_tester;

  // Navigate to an initial page.
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kInitialUrl));
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetWebContents());

  // Start prerendering `kPrerenderingUrl`.
  const GURL kPrerenderingUrl = embedded_test_server()->GetURL("/title1.html");
  content::FrameTreeNodeId host_id =
      prerender_helper().AddPrerender(kPrerenderingUrl);
  content::test::PrerenderHostObserver host_observer(*GetWebContents(),
                                                     host_id);
  prerender_helper().WaitForPrerenderLoadCompletion(kPrerenderingUrl);

  ASSERT_TRUE(prerender_helper().GetHostForUrl(kPrerenderingUrl));

  // Fetch a subresrouce.
  std::string fetch_subresource_script = R"(
        const imgElement = document.createElement('img');
        imgElement.src = '/auth-basic/favicon.gif';
        document.body.appendChild(imgElement);
  )";
  std::ignore = ExecJs(prerender_helper().GetPrerenderedMainFrameHost(host_id),
                       fetch_subresource_script);

  // The prerender should be destroyed.
  host_observer.WaitForDestroyed();
  EXPECT_TRUE(prerender_helper().GetHostForUrl(kPrerenderingUrl).is_null());

  // No authentication request has been prompted to the user.
  EXPECT_EQ(0, browser_client_->auth_needed_count);
}

class LoginPromptBackForwardCacheNoStoreBrowserTest
    : public LoginPromptBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginPromptBrowserTest::SetUpCommandLine(command_line);
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{features::
                                   kCacheControlNoStoreEnterBackForwardCache,
                               {{"level",
                                 "restore-unless-http-only-cookie-change"}}}},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    LoginPromptBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    ukm_recorder_.reset();
    LoginPromptBrowserTest::TearDownOnMainThread();
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void CreateNewTab() {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL("about:blank"),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  }

  base::HistogramTester& histogram_tester() { return *histogram_tester_; }

  void ExpectRestored(base::Location location) {
    EXPECT_THAT(histogram_tester().GetAllSamples(
                    "BackForwardCache.HistoryNavigationOutcome."
                    "NotRestoredReason"),
                testing::UnorderedElementsAreArray(std::vector<base::Bucket>()))
        << location.ToString();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    LoginPromptBackForwardCacheNoStoreBrowserTest,
    ::testing::Values(SplitAuthCacheByNetworkIsolationKey::kFalse,
                      SplitAuthCacheByNetworkIsolationKey::kTrue));

// Test that when HTTP authentication is required in a page, the BFCache entries
// without "Cache-Control: no-store" header will be restored.
IN_PROC_BROWSER_TEST_P(LoginPromptBackForwardCacheNoStoreBrowserTest,
                       TestBasicAuthPromptDoesNotEvictNonCCNSBackForwardCache) {
  // Don't run this test if BackForwardCache is disabled.
  if (!content::BackForwardCache::IsBackForwardCacheFeatureEnabled()) {
    return;
  }

  ASSERT_TRUE(embedded_test_server()->Start());

  GURL auth_page = embedded_test_server()->GetURL(kAuthBasicPage);
  GURL title_page = embedded_test_server()->GetURL(kTitlePage);
  GURL title_page_different_site =
      embedded_test_server()->GetURL("b.com", kTitlePage);

  // Navigate a tab to a non-CCNS page.
  content::WebContents* contents_without_ccns = GetWebContents();
  contents_without_ccns->OpenURL(
      OpenURLParams(title_page, content::Referrer(),
                    WindowOpenDisposition::CURRENT_TAB,
                    ui::PAGE_TRANSITION_TYPED, false),
      /*navigation_handle_callback=*/{});
  content::WaitForLoadStop(contents_without_ccns);
  content::RenderFrameHostWrapper rfh_without_ccns(
      contents_without_ccns->GetPrimaryMainFrame());
  // Verify that the page is eligible for BFCache.
  ASSERT_TRUE(
      content::NavigateToURL(contents_without_ccns, title_page_different_site));
  ASSERT_TRUE(content::HistoryGoBack(contents_without_ccns));
  ExpectRestored(FROM_HERE);
  ASSERT_EQ(rfh_without_ccns.get(),
            contents_without_ccns->GetPrimaryMainFrame());
  // Navigate away for BFCache eviction test.
  ASSERT_TRUE(
      content::NavigateToURL(contents_without_ccns, title_page_different_site));

  // Create a new tab.
  CreateNewTab();
  content::WebContents* contents_auth = GetWebContents();
  ASSERT_NE(contents_auth, contents_without_ccns);

  // Navigate to a page that require HTTP authentication.
  auto auth_needed_waiter = CreateAuthNeededObserver();
  contents_auth->OpenURL(OpenURLParams(auth_page, content::Referrer(),
                                       WindowOpenDisposition::CURRENT_TAB,
                                       ui::PAGE_TRANSITION_TYPED, false),
                         /*navigation_handle_callback=*/{});
  auth_needed_waiter.Wait();
  // Complete the HTTP authentication.
  SetAuthForAndWait(LoginHandler::GetAllLoginHandlersForTest().front());

  // The page without CCNS header should be restored from BFCache.
  ASSERT_TRUE(content::HistoryGoBack(contents_without_ccns));
  ASSERT_EQ(rfh_without_ccns.get(),
            contents_without_ccns->GetPrimaryMainFrame());
}

// Test that when HTTP authentication is required in a page, the BFCache entries
// with "Cache-Control: no-store" header and sharing the same origin as the page
// will be evicted.
IN_PROC_BROWSER_TEST_P(
    LoginPromptBackForwardCacheNoStoreBrowserTest,
    TestBasicAuthPromptEvictsSameOriginCCNSBackForwardCache) {
  // Don't run this test if BackForwardCache is disabled.
  if (!content::BackForwardCache::IsBackForwardCacheFeatureEnabled()) {
    return;
  }

  ASSERT_TRUE(embedded_test_server()->Start());

  GURL auth_page = embedded_test_server()->GetURL(kAuthBasicPage);
  GURL ccns_page = embedded_test_server()->GetURL(kCCNSPage);
  GURL title_page_different_site =
      embedded_test_server()->GetURL("b.com", kTitlePage);

  // Navigate to a CCNS page.
  content::WebContents* contents_with_ccns = GetWebContents();
  contents_with_ccns->OpenURL(OpenURLParams(ccns_page, content::Referrer(),
                                            WindowOpenDisposition::CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false),
                              /*navigation_handle_callback=*/{});
  content::WaitForLoadStop(contents_with_ccns);
  content::RenderFrameHostWrapper rfh_with_ccns(
      contents_with_ccns->GetPrimaryMainFrame());
  // Verify that the page is eligible for BFCache.
  ASSERT_TRUE(
      content::NavigateToURL(contents_with_ccns, title_page_different_site));
  ASSERT_TRUE(content::HistoryGoBack(contents_with_ccns));
  ExpectRestored(FROM_HERE);
  ASSERT_EQ(rfh_with_ccns.get(), contents_with_ccns->GetPrimaryMainFrame());
  // Navigate away for BFCache eviction test.
  ASSERT_TRUE(
      content::NavigateToURL(contents_with_ccns, title_page_different_site));

  // Create a new tab.
  CreateNewTab();
  content::WebContents* contents_auth = GetWebContents();
  ASSERT_NE(contents_auth, contents_with_ccns);

  // Navigate to a page that require HTTP authentication.
  auto auth_needed_waiter = CreateAuthNeededObserver();
  contents_auth->OpenURL(OpenURLParams(auth_page, content::Referrer(),
                                       WindowOpenDisposition::CURRENT_TAB,
                                       ui::PAGE_TRANSITION_TYPED, false),
                         /*navigation_handle_callback=*/{});
  auth_needed_waiter.Wait();
  // Complete the HTTP authentication.
  SetAuthForAndWait(LoginHandler::GetAllLoginHandlersForTest().front());

  // The page with CCNS header should be evicted.
  ASSERT_TRUE(rfh_with_ccns.WaitUntilRenderFrameDeleted());
}

// Test that when HTTP authentication is required in a page, the BFCache entries
// with "Cache-Control: no-store" header but not sharing the same origin as the
// page will be restored.
IN_PROC_BROWSER_TEST_P(
    LoginPromptBackForwardCacheNoStoreBrowserTest,
    TestBasicAuthPromptDoesNotEvictDifferentOriginCCNSBackForwardCache) {
  // Don't run this test if BackForwardCache is disabled.
  if (!content::BackForwardCache::IsBackForwardCacheFeatureEnabled()) {
    return;
  }

  ASSERT_TRUE(embedded_test_server()->Start());

  GURL auth_page = embedded_test_server()->GetURL(kAuthBasicPage);
  GURL ccns_page_different_site =
      embedded_test_server()->GetURL("a.com", kCCNSPage);
  GURL title_page_different_site =
      embedded_test_server()->GetURL("b.com", kTitlePage);

  // Navigate to a CCNS page with a different origin.
  content::WebContents* contents_different_site_with_ccns = GetWebContents();
  contents_different_site_with_ccns->OpenURL(
      OpenURLParams(ccns_page_different_site, content::Referrer(),
                    WindowOpenDisposition::CURRENT_TAB,
                    ui::PAGE_TRANSITION_TYPED, false),
      /*navigation_handle_callback=*/{});
  content::WaitForLoadStop(contents_different_site_with_ccns);
  content::RenderFrameHostWrapper rfh_different_site_with_ccns(
      contents_different_site_with_ccns->GetPrimaryMainFrame());
  // Verify that the page is eligible for BFCache.
  ASSERT_TRUE(content::NavigateToURL(contents_different_site_with_ccns,
                                     title_page_different_site));
  ASSERT_TRUE(content::HistoryGoBack(contents_different_site_with_ccns));
  ExpectRestored(FROM_HERE);
  ASSERT_EQ(rfh_different_site_with_ccns.get(),
            contents_different_site_with_ccns->GetPrimaryMainFrame());
  // Navigate away for BFCache eviction test.
  ASSERT_TRUE(content::NavigateToURL(contents_different_site_with_ccns,
                                     title_page_different_site));

  // Create a new tab.
  CreateNewTab();
  content::WebContents* contents_auth = GetWebContents();
  ASSERT_NE(contents_auth, contents_different_site_with_ccns);

  // Navigate to a page that require HTTP authentication.
  auto auth_needed_waiter = CreateAuthNeededObserver();
  contents_auth->OpenURL(OpenURLParams(auth_page, content::Referrer(),
                                       WindowOpenDisposition::CURRENT_TAB,
                                       ui::PAGE_TRANSITION_TYPED, false),
                         /*navigation_handle_callback=*/{});
  auth_needed_waiter.Wait();
  // Complete the HTTP authentication.
  SetAuthForAndWait(LoginHandler::GetAllLoginHandlersForTest().front());

  // The page with CCNS header but with a different origin should be restored
  // from the BFCache.
  ASSERT_TRUE(content::HistoryGoBack(contents_different_site_with_ccns));
  ASSERT_EQ(rfh_different_site_with_ccns.get(),
            contents_different_site_with_ccns->GetPrimaryMainFrame());
}
}  // namespace
