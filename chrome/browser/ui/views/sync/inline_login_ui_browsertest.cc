// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/sync/one_click_signin_dialog_view.h"
#include "chrome/browser/ui/webui/signin/inline_login_handler_impl.h"
#include "chrome/browser/ui/webui/signin/inline_login_ui.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"
#include "chrome/browser/ui/webui/signin/signin_utils_desktop.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/test_chrome_web_ui_controller_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "google_apis/gaia/fake_gaia.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_response_headers.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN)
#include "chrome/credential_provider/common/gcp_strings.h"
#endif  // defined(OS_WIN)

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;

using guest_view::GuestViewManager;
using login_ui_test_utils::ExecuteJsToSigninInSigninFrame;
using login_ui_test_utils::WaitUntilUIReady;

namespace {

struct ContentInfo {
  ContentInfo(content::WebContents* contents,
              int pid,
              content::StoragePartition* storage_partition) {
    this->contents = contents;
    this->pid = pid;
    this->storage_partition = storage_partition;
  }

  content::WebContents* contents;
  int pid;
  content::StoragePartition* storage_partition;
};

ContentInfo NavigateAndGetInfo(Browser* browser,
                               const GURL& url,
                               WindowOpenDisposition disposition) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser, url, disposition,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  content::WebContents* contents =
      browser->tab_strip_model()->GetActiveWebContents();
  content::RenderProcessHost* process = contents->GetMainFrame()->GetProcess();
  return ContentInfo(contents, process->GetID(),
                     process->GetStoragePartition());
}

// Returns a new WebUI object for the WebContents from |arg0|.
ACTION(ReturnNewWebUI) {
  return std::make_unique<content::WebUIController>(arg0);
}

GURL GetSigninPromoURL() {
  return signin::GetEmbeddedPromoURL(
      signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE,
      signin_metrics::Reason::REASON_FORCED_SIGNIN_PRIMARY_ACCOUNT, false);
}

// Mock the TestChromeWebUIControllerFactory::WebUIProvider to prove that we are
// not called as expected.
class FooWebUIProvider
    : public TestChromeWebUIControllerFactory::WebUIProvider {
 public:
  MOCK_METHOD2(NewWebUI,
               std::unique_ptr<content::WebUIController>(content::WebUI* web_ui,
                                                         const GURL& url));
};

bool AddToSet(std::set<content::WebContents*>* set,
              content::WebContents* web_contents) {
  set->insert(web_contents);
  return false;
}

std::unique_ptr<net::test_server::HttpResponse> EmptyHtmlResponseHandler(
    const net::test_server::HttpRequest& request) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content_type("text/html");
  http_response->set_content(
      "<html><head><link rel=manifest href=/manifest.json></head></html>");
  return std::move(http_response);
}

// This class is used to mock out virtual methods with side effects so that
// tests below can ensure they are called without causing side effects.
class MockInlineSigninHelper : public InlineSigninHelper {
 public:
  MockInlineSigninHelper(
      base::WeakPtr<InlineLoginHandlerImpl> handler,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      Profile* profile,
      const GURL& current_url,
      const std::string& email,
      const std::string& gaia_id,
      const std::string& password,
      const std::string& auth_code,
      const std::string& signin_scoped_device_id,
      bool confirm_untrusted_signin);

  MOCK_METHOD1(OnClientOAuthSuccess, void(const ClientOAuthResult& result));
  MOCK_METHOD1(OnClientOAuthFailure, void(const GoogleServiceAuthError& error));
  MOCK_METHOD1(CreateSyncStarter, void(const std::string&));

  GaiaAuthFetcher* GetGaiaAuthFetcher() { return GetGaiaAuthFetcherForTest(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockInlineSigninHelper);
};

MockInlineSigninHelper::MockInlineSigninHelper(
    base::WeakPtr<InlineLoginHandlerImpl> handler,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    Profile* profile,
    const GURL& current_url,
    const std::string& email,
    const std::string& gaia_id,
    const std::string& password,
    const std::string& auth_code,
    const std::string& signin_scoped_device_id,
    bool confirm_untrusted_signin)
    : InlineSigninHelper(handler,
                         url_loader_factory,
                         profile,
                         Profile::CreateStatus::CREATE_STATUS_INITIALIZED,
                         current_url,
                         email,
                         gaia_id,
                         password,
                         auth_code,
                         signin_scoped_device_id,
                         confirm_untrusted_signin,
                         false) {}

// This class is used to mock out virtual methods with side effects so that
// tests below can ensure they are called without causing side effects.
class MockSyncStarterInlineSigninHelper : public InlineSigninHelper {
 public:
  MockSyncStarterInlineSigninHelper(
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
      bool is_force_sign_in_with_usermanager);

  MOCK_METHOD1(CreateSyncStarter, void(const std::string&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSyncStarterInlineSigninHelper);
};

MockSyncStarterInlineSigninHelper::MockSyncStarterInlineSigninHelper(
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
    : InlineSigninHelper(handler,
                         url_loader_factory,
                         profile,
                         Profile::CreateStatus::CREATE_STATUS_INITIALIZED,
                         current_url,
                         email,
                         gaia_id,
                         password,
                         auth_code,
                         signin_scoped_device_id,
                         confirm_untrusted_signin,
                         is_force_sign_in_with_usermanager) {}

}  // namespace

class InlineLoginUIBrowserTest : public InProcessBrowserTest {
 public:
  InlineLoginUIBrowserTest() {}
  void EnableSigninAllowed(bool enable);
  void AddEmailToOneClickRejectedList(const std::string& email);
  void AllowSigninCookies(bool enable);
  void SetAllowedUsernamePattern(const std::string& pattern);

 protected:
  content::WebContents* web_contents() { return nullptr; }
};

void InlineLoginUIBrowserTest::EnableSigninAllowed(bool enable) {
  PrefService* pref_service = browser()->profile()->GetPrefs();
  pref_service->SetBoolean(prefs::kSigninAllowed, enable);
}

void InlineLoginUIBrowserTest::AddEmailToOneClickRejectedList(
    const std::string& email) {
  PrefService* pref_service = browser()->profile()->GetPrefs();
  ListPrefUpdate updater(pref_service,
                         prefs::kReverseAutologinRejectedEmailList);
  updater->AppendIfNotPresent(std::make_unique<base::Value>(email));
}

void InlineLoginUIBrowserTest::AllowSigninCookies(bool enable) {
  content_settings::CookieSettings* cookie_settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  cookie_settings->SetDefaultCookieSetting(enable ? CONTENT_SETTING_ALLOW
                                                  : CONTENT_SETTING_BLOCK);
}

void InlineLoginUIBrowserTest::SetAllowedUsernamePattern(
    const std::string& pattern) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kGoogleServicesUsernamePattern, pattern);
}

#if defined(OS_LINUX) || defined(OS_WIN)
// crbug.com/422868
#define MAYBE_DifferentStorageId DISABLED_DifferentStorageId
#else
#define MAYBE_DifferentStorageId DifferentStorageId
#endif
IN_PROC_BROWSER_TEST_F(InlineLoginUIBrowserTest, MAYBE_DifferentStorageId) {
  ContentInfo info = NavigateAndGetInfo(browser(), GetSigninPromoURL(),
                                        WindowOpenDisposition::CURRENT_TAB);
  WaitUntilUIReady(browser());

  // Make sure storage partition of embedded webview is different from
  // parent.
  std::set<content::WebContents*> set;
  GuestViewManager* manager =
      GuestViewManager::FromBrowserContext(info.contents->GetBrowserContext());
  manager->ForEachGuest(info.contents, base::BindRepeating(&AddToSet, &set));
  ASSERT_EQ(1u, set.size());
  content::WebContents* webview_contents = *set.begin();
  content::RenderProcessHost* process =
      webview_contents->GetMainFrame()->GetProcess();
  ASSERT_NE(info.pid, process->GetID());
  ASSERT_NE(info.storage_partition, process->GetStoragePartition());
}

IN_PROC_BROWSER_TEST_F(InlineLoginUIBrowserTest, OneProcessLimit) {
  GURL test_url_1 = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html")));
  GURL test_url_2 = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory)
          .Append(FILE_PATH_LITERAL("frame_tree")),
      base::FilePath(FILE_PATH_LITERAL("simple.htm")));

  // Even when the process limit is set to one, the signin process should
  // still be given its own process and storage partition.
  content::RenderProcessHost::SetMaxRendererProcessCount(1);

  ContentInfo info1 = NavigateAndGetInfo(browser(), test_url_1,
                                         WindowOpenDisposition::CURRENT_TAB);
  ContentInfo info2 = NavigateAndGetInfo(browser(), test_url_2,
                                         WindowOpenDisposition::CURRENT_TAB);
  ContentInfo info3 = NavigateAndGetInfo(browser(), GetSigninPromoURL(),
                                         WindowOpenDisposition::CURRENT_TAB);

  ASSERT_EQ(info1.pid, info2.pid);
  ASSERT_NE(info1.pid, info3.pid);
}

IN_PROC_BROWSER_TEST_F(InlineLoginUIBrowserTest, CanOfferNoProfile) {
  std::string error_message;
  EXPECT_FALSE(CanOfferSignin(NULL, CAN_OFFER_SIGNIN_FOR_ALL_ACCOUNTS, "12345",
                              "user@gmail.com", &error_message));
  EXPECT_EQ("", error_message);
}

IN_PROC_BROWSER_TEST_F(InlineLoginUIBrowserTest, CanOffer) {
  EXPECT_TRUE(CanOfferSignin(browser()->profile(),
                             CAN_OFFER_SIGNIN_FOR_ALL_ACCOUNTS, "12345",
                             "user@gmail.com", NULL));

  std::string error_message;

  EXPECT_TRUE(CanOfferSignin(browser()->profile(),
                             CAN_OFFER_SIGNIN_FOR_ALL_ACCOUNTS, "12345",
                             "user@gmail.com", &error_message));
}

IN_PROC_BROWSER_TEST_F(InlineLoginUIBrowserTest, CanOfferProfileConnected) {
  auto* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  signin::MakePrimaryAccountAvailable(identity_manager, "foo@gmail.com");
  EnableSigninAllowed(true);

  std::string error_message;

  EXPECT_TRUE(CanOfferSignin(browser()->profile(),
                             CAN_OFFER_SIGNIN_FOR_ALL_ACCOUNTS, "12345",
                             "foo@gmail.com", &error_message));
  EXPECT_TRUE(CanOfferSignin(browser()->profile(),
                             CAN_OFFER_SIGNIN_FOR_ALL_ACCOUNTS, "12345", "foo",
                             &error_message));
  EXPECT_FALSE(CanOfferSignin(browser()->profile(),
                              CAN_OFFER_SIGNIN_FOR_ALL_ACCOUNTS, "12345",
                              "user@gmail.com", &error_message));
  EXPECT_EQ(l10n_util::GetStringFUTF8(IDS_SYNC_WRONG_EMAIL,
                                      base::UTF8ToUTF16("foo@gmail.com")),
            error_message);
}

IN_PROC_BROWSER_TEST_F(InlineLoginUIBrowserTest, CanOfferUsernameNotAllowed) {
  SetAllowedUsernamePattern("*.google.com");

  std::string error_message;
  EXPECT_FALSE(CanOfferSignin(browser()->profile(),
                              CAN_OFFER_SIGNIN_FOR_ALL_ACCOUNTS, "12345",
                              "foo@gmail.com", &error_message));
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_SYNC_LOGIN_NAME_PROHIBITED),
            error_message);
}

IN_PROC_BROWSER_TEST_F(InlineLoginUIBrowserTest, CanOfferWithRejectedEmail) {
  EnableSigninAllowed(true);

  AddEmailToOneClickRejectedList("foo@gmail.com");
  AddEmailToOneClickRejectedList("user@gmail.com");

  std::string error_message;
  EXPECT_TRUE(CanOfferSignin(browser()->profile(),
                             CAN_OFFER_SIGNIN_FOR_ALL_ACCOUNTS, "12345",
                             "foo@gmail.com", &error_message));
  EXPECT_TRUE(CanOfferSignin(browser()->profile(),
                             CAN_OFFER_SIGNIN_FOR_ALL_ACCOUNTS, "12345",
                             "user@gmail.com", &error_message));
}

IN_PROC_BROWSER_TEST_F(InlineLoginUIBrowserTest, CanOfferNoSigninCookies) {
  AllowSigninCookies(false);
  EnableSigninAllowed(true);

  std::string error_message;
  EXPECT_FALSE(CanOfferSignin(browser()->profile(),
                              CAN_OFFER_SIGNIN_FOR_ALL_ACCOUNTS, "12345",
                              "user@gmail.com", &error_message));
  EXPECT_EQ("", error_message);
}

class InlineLoginHelperBrowserTest : public InProcessBrowserTest {
 public:
  InlineLoginHelperBrowserTest() {
    signin_util::SetForceSigninForTesting(true);
  }

  ~InlineLoginHelperBrowserTest() override {
    signin_util::ResetForceSigninForTesting();
  }

  void SetUpInProcessBrowserTestFixture() override {
    will_create_browser_context_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterWillCreateBrowserContextServicesCallbackForTesting(
                base::Bind(&InlineLoginHelperBrowserTest::
                               OnWillCreateBrowserContextServices,
                           base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

  void SetUp() override {
    // Don't spin up the IO thread yet since no threads are allowed while
    // spawning sandbox host process. See crbug.com/322732.
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    const GURL& base_url = embedded_test_server()->base_url();
    command_line->AppendSwitchASCII(::switches::kGaiaUrl, base_url.spec());
    command_line->AppendSwitchASCII(::switches::kLsoUrl, base_url.spec());
    command_line->AppendSwitchASCII(::switches::kGoogleApisUrl,
                                    base_url.spec());
  }

  Profile* profile() { return profile_; }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");

    oauth2_token_exchange_success_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            embedded_test_server(),
            GaiaUrls::GetInstance()->oauth2_token_url().path(),
            /*relative_url_is_prefix=*/true);

    embedded_test_server()->StartAcceptingConnections();

    // Grab references to the fake signin manager and token service.
    ASSERT_GT(g_browser_process->profile_manager()->GetLoadedProfiles().size(),
              0u);
    profile_ = g_browser_process->profile_manager()->GetLoadedProfiles()[0];
    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_);
  }

  void TearDownOnMainThread() override {
    identity_test_env_profile_adaptor_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SimulateStartAuthCodeForOAuth2TokenExchangeSuccess(
      const std::string& json_response) {
    oauth2_token_exchange_success_->WaitForRequest();
    oauth2_token_exchange_success_->Send(
        net::HTTP_OK, "application/json; charset=utf-8", json_response);
    oauth2_token_exchange_success_->Done();
  }

  void SimulateOnClientOAuthSuccess(GaiaAuthConsumer* consumer,
                                    const std::string& refresh_token) {
    GaiaAuthConsumer::ClientOAuthResult result(refresh_token, "", 0, false,
                                               false);
    consumer->OnClientOAuthSuccess(result);
    base::RunLoop().RunUntilIdle();
  }

  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory() {
    return content::BrowserContext::GetDefaultStoragePartition(profile_)
        ->GetURLLoaderFactoryForBrowserProcess();
  }

 protected:
  signin::IdentityManager* identity_manager() {
    return identity_test_env_profile_adaptor_->identity_test_env()
        ->identity_manager();
  }

  std::unique_ptr<net::test_server::ControllableHttpResponse>
      oauth2_token_exchange_success_;

 private:
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  std::unique_ptr<
      base::CallbackList<void(content::BrowserContext*)>::Subscription>
      will_create_browser_context_services_subscription_;
  Profile* profile_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(InlineLoginHelperBrowserTest);
};

// Test signin helper calls correct fetcher methods when called with an
// auth code.
IN_PROC_BROWSER_TEST_F(InlineLoginHelperBrowserTest, WithAuthCode) {
  InlineLoginHandlerImpl handler;
  MockInlineSigninHelper helper(
      handler.GetWeakPtr(), test_shared_loader_factory(), profile(), GURL(),
      "foo@gmail.com", "gaiaid-12345", "password", "auth_code",
      /*signin_scoped_device_id=*/std::string(),
      /*confirm_untrusted_signin=*/false);
  base::RunLoop run_loop;
  EXPECT_CALL(helper, OnClientOAuthSuccess(_))
      .WillOnce(testing::InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));

  SimulateStartAuthCodeForOAuth2TokenExchangeSuccess(
      R"({
           "access_token": "access_token",
           "expires_in": 1234567890,
           "refresh_token": "refresh_token"
         })");
  run_loop.Run();
}

// Test signin helper creates sync starter with correct confirmation when
// signing in with default sync options.
IN_PROC_BROWSER_TEST_F(InlineLoginHelperBrowserTest,
                       SigninCreatesSyncStarter1) {
  InlineLoginHandlerImpl handler;
  // See Source enum in components/signin/public/base/signin_metrics.h for
  // possible values of access_point=, reason=.
  GURL url("chrome://chrome-signin/?access_point=0&reason=5");
  // MockSyncStarterInlineSigninHelper will delete itself when done using
  // base::ThreadTaskRunnerHandle::DeleteSoon(), so need to delete here.  But
  // do need the RunUntilIdle() at the end.
  MockSyncStarterInlineSigninHelper* helper =
      new MockSyncStarterInlineSigninHelper(
          handler.GetWeakPtr(),
          content::BrowserContext::GetDefaultStoragePartition(profile())
              ->GetURLLoaderFactoryForBrowserProcess(),
          profile(), url, "foo@gmail.com", "gaiaid-12345", "password",
          "auth_code", /*signin_scoped_device_id=*/std::string(),
          /*confirm_untrusted_signin=*/false,
          /*is_force_sign_in_with_usermanager=*/false);
  EXPECT_CALL(*helper, CreateSyncStarter("refresh_token"));

  ProfileAttributesEntry* entry;
  ASSERT_TRUE(g_browser_process->profile_manager()
                  ->GetProfileAttributesStorage()
                  .GetProfileAttributesWithPath(profile()->GetPath(), &entry));
  entry->SetIsSigninRequired(true);

  ASSERT_EQ(0ul, BrowserList::GetInstance()->size());
  SimulateOnClientOAuthSuccess(helper, "refresh_token");
  ASSERT_EQ(0ul, BrowserList::GetInstance()->size());
  // if |force_sign_in_with_user_manager| is false, the profile should be
  // unlocked early and InlineLoginHelper won't try to do it again
  ASSERT_TRUE(entry->IsSigninRequired());
}

// Test signin helper creates sync starter with correct confirmation when
// signing in and choosing what to sync first.
IN_PROC_BROWSER_TEST_F(InlineLoginHelperBrowserTest,
                       SigninCreatesSyncStarter2) {
  InlineLoginHandlerImpl handler;
  // See Source enum in components/signin/public/base/signin_metrics.h for
  // possible values of access_point=, reason=.
  const GURL url("chrome://chrome-signin/?access_point=0&reason=5");
  // MockSyncStarterInlineSigninHelper will delete itself when done using
  // base::ThreadTaskRunnerHandle::DeleteSoon(), so need to delete here.  But
  // do need the RunUntilIdle() at the end.
  MockSyncStarterInlineSigninHelper* helper =
      new MockSyncStarterInlineSigninHelper(
          handler.GetWeakPtr(), test_shared_loader_factory(), profile(), url,
          "foo@gmail.com", "gaiaid-12345", "password", "auth_code",
          /*signin_scoped_device_id=*/std::string(),
          /*confirm_untrusted_signin=*/false,
          /*is_force_sign_in_with_usermanager=*/false);
  EXPECT_CALL(*helper, CreateSyncStarter("refresh_token"));

  SimulateOnClientOAuthSuccess(helper, "refresh_token");
}

// Test signin helper creates the untrusted signin dialog, and signin aborts
// when the user cancels.
IN_PROC_BROWSER_TEST_F(InlineLoginHelperBrowserTest,
                       UntrustedSigninDialogCancel) {
  InlineLoginHandlerImpl handler;
  // See Source enum in components/signin/public/base/signin_metrics.h for
  // possible values of access_point=, reason=.
  GURL url("chrome://chrome-signin/?access_point=0&reason=5");
  // MockSyncStarterInlineSigninHelper will delete itself when done using
  // base::ThreadTaskRunnerHandle::DeleteSoon(), so need to delete here.  But
  // do need the RunUntilIdle() at the end.
  MockSyncStarterInlineSigninHelper* helper =
      new MockSyncStarterInlineSigninHelper(
          handler.GetWeakPtr(), test_shared_loader_factory(), profile(), url,
          "foo@gmail.com", "gaiaid-12345", "password", "auth_code",
          /*signin_scoped_device_id=*/std::string(),
          /*confirm_untrusted_signin=*/true,
          /*is_force_sign_in_with_usermanager=*/true);
  SimulateOnClientOAuthSuccess(helper, "refresh_token");
  EXPECT_TRUE(OneClickSigninDialogView::IsShowing());
  OneClickSigninDialogView::Hide();

  base::RunLoop().RunUntilIdle();
}

// Test signin helper creates the untrusted signin dialog, and signin continues
// when the user confirms.
IN_PROC_BROWSER_TEST_F(InlineLoginHelperBrowserTest,
                       UntrustedSigninDialogConfirm) {
  InlineLoginHandlerImpl handler;
  // See Source enum in components/signin/public/base/signin_metrics.h for
  // possible values of access_point=, reason=.
  GURL url("chrome://chrome-signin/?access_point=0&reason=5");
  // MockSyncStarterInlineSigninHelper will delete itself when done using
  // base::ThreadTaskRunnerHandle::DeleteSoon(), so need to delete here.  But
  // do need the RunUntilIdle() at the end.
  MockSyncStarterInlineSigninHelper* helper =
      new MockSyncStarterInlineSigninHelper(
          handler.GetWeakPtr(), test_shared_loader_factory(), profile(), url,
          "foo@gmail.com", "gaiaid-12345", "password", "auth_code",
          /*signin_scoped_device_id=*/std::string(),
          /*confirm_untrusted_signin=*/true,
          /*is_force_sign_in_with_usermanager=*/true);
  EXPECT_CALL(*helper, CreateSyncStarter("refresh_token"));
  SimulateOnClientOAuthSuccess(helper, "refresh_token");
  EXPECT_TRUE(OneClickSigninDialogView::IsShowing());
  views::DialogDelegateView* dialog_delegate =
      OneClickSigninDialogView::view_for_testing();
  dialog_delegate->Accept();

  base::RunLoop().RunUntilIdle();
}

// Test signin helper creates sync starter with correct confirmation during
// re-auth.
IN_PROC_BROWSER_TEST_F(InlineLoginHelperBrowserTest,
                       SigninCreatesSyncStarter4) {
  InlineLoginHandlerImpl handler;
  // See Source enum in components/signin/public/base/signin_metrics.h for
  // possible values of access_point=, reason=.
  const GURL url("chrome://chrome-signin/?access_point=3&reason=5");
  // MockSyncStarterInlineSigninHelper will delete itself when done using
  // base::ThreadTaskRunnerHandle::DeleteSoon(), so need to delete here.  But
  // do need the RunUntilIdle() at the end.
  MockSyncStarterInlineSigninHelper* helper =
      new MockSyncStarterInlineSigninHelper(
          handler.GetWeakPtr(), test_shared_loader_factory(), profile(), url,
          "foo@gmail.com", "gaiaid-12345", "password", "auth_code",
          /*signin_scoped_device_id=*/std::string(),
          /*confirm_untrusted_signin=*/false,
          /*is_force_sign_in_with_usermanager=*/false);

  // Even though "choose what to sync" is false, the source of the URL is
  // settings, which means the user wants to CONFIGURE_SYNC_FIRST.
  EXPECT_CALL(*helper, CreateSyncStarter("refresh_token"));

  SimulateOnClientOAuthSuccess(helper, "refresh_token");
}

// Test signin helper does not create sync starter when reauthenticating.
IN_PROC_BROWSER_TEST_F(InlineLoginHelperBrowserTest,
                       ReauthCallsUpdateCredentials) {
  ASSERT_EQ(0ul, identity_manager()->GetAccountsWithRefreshTokens().size());

  InlineLoginHandlerImpl handler;
  // See Source enum in components/signin/public/base/signin_metrics.h for
  // possible values of access_point=, reason=.
  GURL url("chrome://chrome-signin/?access_point=3&reason=3");
  // InlineSigninHelper will delete itself when done using
  // base::ThreadTaskRunnerHandle::DeleteSoon(), so need to delete here.  But
  // do need the RunUntilIdle() at the end.
  InlineSigninHelper* helper = new InlineSigninHelper(
      handler.GetWeakPtr(), test_shared_loader_factory(), profile(),
      Profile::CreateStatus::CREATE_STATUS_INITIALIZED, url, "foo@gmail.com",
      "gaiaid-12345", "password", "auth_code",
      /*signin_scoped_device_id=*/std::string(),
      /*confirm_untrusted_signin=*/false,
      /*is_force_sign_in_with_usermanager=*/false);
  SimulateOnClientOAuthSuccess(helper, "refresh_token");
  ASSERT_EQ(1ul, identity_manager()->GetAccountsWithRefreshTokens().size());
}

IN_PROC_BROWSER_TEST_F(InlineLoginHelperBrowserTest,
                       ForceSigninWithUserManager) {
  InlineLoginHandlerImpl handler;
  GURL url("chrome://chrome-signin/?access_point=0&reason=5");
  // MockSyncStarterInlineSigninHelper will delete itself when done using
  // base::ThreadTaskRunnerHandle::DeleteSoon(), so need to delete here.  But
  // do need the RunUntilIdle() at the end.
  MockSyncStarterInlineSigninHelper* helper =
      new MockSyncStarterInlineSigninHelper(
          handler.GetWeakPtr(), test_shared_loader_factory(), profile(), url,
          "foo@gmail.com", "gaiaid-12345", "password", "auth_code",
          /*signin_scoped_device_id=*/std::string(),
          /*confirm_untrusted_signin=*/false,
          /*is_force_sign_in_with_usermanager=*/true);
  EXPECT_CALL(*helper, CreateSyncStarter("refresh_token"));

  ProfileAttributesEntry* entry;
  ASSERT_TRUE(g_browser_process->profile_manager()
                  ->GetProfileAttributesStorage()
                  .GetProfileAttributesWithPath(profile()->GetPath(), &entry));
  entry->SetIsSigninRequired(true);

  ASSERT_EQ(0ul, BrowserList::GetInstance()->size());
  SimulateOnClientOAuthSuccess(helper, "refresh_token");
  ASSERT_EQ(1ul, BrowserList::GetInstance()->size());
  ASSERT_FALSE(entry->IsSigninRequired());
}

class InlineLoginUISafeIframeBrowserTest : public InProcessBrowserTest {
 public:
  FooWebUIProvider& foo_provider() { return foo_provider_; }

 private:
  void SetUp() override {
    embedded_test_server()->RegisterRequestHandler(
        base::Bind(&EmptyHtmlResponseHandler));

    // Don't spin up the IO thread yet since no threads are allowed while
    // spawning sandbox host process. See crbug.com/322732.
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    const GURL& base_url = embedded_test_server()->base_url();
    command_line->AppendSwitchASCII(::switches::kGaiaUrl, base_url.spec());
    command_line->AppendSwitchASCII(::switches::kLsoUrl, base_url.spec());
    command_line->AppendSwitchASCII(::switches::kGoogleApisUrl,
                                    base_url.spec());
  }

  void SetUpOnMainThread() override {
    embedded_test_server()->StartAcceptingConnections();

    content::WebUIControllerFactory::UnregisterFactoryForTesting(
        ChromeWebUIControllerFactory::GetInstance());
    test_factory_ = std::make_unique<TestChromeWebUIControllerFactory>();
    content::WebUIControllerFactory::RegisterFactory(test_factory_.get());
    test_factory_->AddFactoryOverride(content::GetWebUIURL("foo/").host(),
                                      &foo_provider_);
  }

  void TearDownOnMainThread() override {
    test_factory_->RemoveFactoryOverride(content::GetWebUIURL("foo/").host());
    content::WebUIControllerFactory::UnregisterFactoryForTesting(
        test_factory_.get());
    test_factory_.reset();
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  }

  FooWebUIProvider foo_provider_;
  std::unique_ptr<TestChromeWebUIControllerFactory> test_factory_;
};

// Make sure that the foo webui handler is working properly and that it gets
// created when navigated to normally.
IN_PROC_BROWSER_TEST_F(InlineLoginUISafeIframeBrowserTest, Basic) {
  const GURL kUrl(content::GetWebUIURL("foo/"));
  EXPECT_CALL(foo_provider(), NewWebUI(_, ::testing::Eq(kUrl)))
      .WillOnce(ReturnNewWebUI());
  ui_test_utils::NavigateToURL(browser(), content::GetWebUIURL("foo/"));
}

// Flaky on MacOS - crbug.com/1021209
#if defined(OS_MACOSX)
#define MAYBE_NoWebUIInIframe DISABLED_NoWebUIInIframe
#else
#define MAYBE_NoWebUIInIframe NoWebUIInIframe
#endif
// Make sure that the foo webui handler does not get created when we try to
// load it inside the iframe of the login ui.
IN_PROC_BROWSER_TEST_F(InlineLoginUISafeIframeBrowserTest,
                       MAYBE_NoWebUIInIframe) {
  GURL url = GetSigninPromoURL().Resolve(
      "?source=0&access_point=0&reason=5&frameUrl=chrome://foo");
  EXPECT_CALL(foo_provider(), NewWebUI(_, _)).Times(0);
  ui_test_utils::NavigateToURL(browser(), url);
}

// Make sure that the gaia iframe cannot trigger top-frame navigation.
IN_PROC_BROWSER_TEST_F(InlineLoginUISafeIframeBrowserTest,
                       TopFrameNavigationDisallowed) {
  // Loads into gaia iframe a web page that attempts to deframe on load.
  GURL deframe_url(embedded_test_server()->GetURL("/login/deframe.html"));
  GURL url(net::AppendOrReplaceQueryParameter(GetSigninPromoURL(), "frameUrl",
                                              deframe_url.spec()));
  ui_test_utils::NavigateToURL(browser(), url);
  WaitUntilUIReady(browser());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(url, contents->GetVisibleURL());

  content::NavigationController& controller = contents->GetController();
  EXPECT_TRUE(controller.GetPendingEntry() == NULL);
}

// Tracks the URLs requested while running a browser test and returns a default
// empty html page as a result. Each URL + path tracks all the query params
// requested to this endpoint for validation later on.
class HtmlRequestTracker {
 public:
  HtmlRequestTracker() = default;
  ~HtmlRequestTracker() = default;
  std::unique_ptr<net::test_server::HttpResponse> HtmlResponseHandler(
      const net::test_server::HttpRequest& request) {
    // Track the query keyed on the host + path portion of the URL.
    std::vector<std::pair<std::string, std::string>> query_params;
    GURL request_url = request.GetURL();
    net::QueryIterator it(request_url);
    for (; !it.IsAtEnd(); it.Advance()) {
      query_params.push_back(
          std::make_pair(it.GetKey(), it.GetUnescapedValue()));
    }
    requested_urls_[GURL(request.GetURL().GetWithEmptyPath().Resolve(
                        request.GetURL().path()))]
        .push_back(query_params);

    return EmptyHtmlResponseHandler(request);
  }

  bool PageRequested(const GURL& url) { return PageRequested(url, {}); }

  bool PageRequested(const GURL& url,
                     const std::vector<std::pair<std::string, std::string>>&
                         required_query_params) {
    auto it = requested_urls_.find(url.GetWithEmptyPath().Resolve(url.path()));

    if (it == requested_urls_.end())
      return false;

    if (required_query_params.empty())
      return true;

    // Go to every query made on this endpoint and see if one of them matches
    // the required query params.
    for (auto& query_param : required_query_params) {
      bool query_params_match = true;
      for (auto& requested_query_params : it->second) {
        if (std::find_if(requested_query_params.begin(),
                         requested_query_params.end(),
                         [&query_param](auto& lhs) {
                           return base::EqualsCaseInsensitiveASCII(
                                      query_param.first, lhs.first) &&
                                  base::EqualsCaseInsensitiveASCII(
                                      query_param.second, lhs.second);
                         }) == requested_query_params.end()) {
          query_params_match = false;
          break;
        }
      }

      if (query_params_match)
        return true;
    }

    return false;
  }

 private:
  std::map<GURL, std::vector<std::vector<std::pair<std::string, std::string>>>>
      requested_urls_;
};

// Tests whether the correct gaia url and query parameters are requested based
// on the signin reason.
class InlineLoginCorrectGaiaUrlBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUp() override {
    // Track all the requests through the |tracker_| and return an empty html
    // page to the browser that is running.
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &HtmlRequestTracker::HtmlResponseHandler, base::Unretained(&tracker_)));

    // Don't spin up the IO thread yet since no threads are allowed while
    // spawning sandbox host process. See crbug.com/322732.
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Redirect all gaia requests to the test server that is running.
    const GURL& base_url = embedded_test_server()->base_url();
    command_line->AppendSwitchASCII(::switches::kGaiaUrl, base_url.spec());
    command_line->AppendSwitchASCII(::switches::kLsoUrl, base_url.spec());
    command_line->AppendSwitchASCII(::switches::kGoogleApisUrl,
                                    base_url.spec());
  }

  void SetUpOnMainThread() override {
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  }

  HtmlRequestTracker tracker_;
};

#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(InlineLoginCorrectGaiaUrlBrowserTest,
                       FetchLstOnlyEndpointForSignin) {
  signin_metrics::AccessPoint access_point =
      signin_metrics::AccessPoint::ACCESS_POINT_MACHINE_LOGON;
  signin_metrics::Reason reason = signin_metrics::Reason::REASON_FETCH_LST_ONLY;

  auto signin_url = signin::GetEmbeddedPromoURL(access_point, reason, false);
  ui_test_utils::NavigateToURL(browser(), signin_url);

  WaitUntilUIReady(browser());

  // Expected gaia endpoint to load.
  GURL gaia_url = GaiaUrls::GetInstance()->embedded_setup_windows_url();

  EXPECT_TRUE(tracker_.PageRequested(gaia_url, {{"flow", "signin"}}));
}

IN_PROC_BROWSER_TEST_F(InlineLoginCorrectGaiaUrlBrowserTest,
                       FetchLstOnlyEndpointForReauth) {
  signin_metrics::AccessPoint access_point =
      signin_metrics::AccessPoint::ACCESS_POINT_MACHINE_LOGON;
  signin_metrics::Reason reason = signin_metrics::Reason::REASON_FETCH_LST_ONLY;

  static const std::string email = "foo@gmail.com";
  auto signin_url =
      signin::GetEmbeddedReauthURLWithEmail(access_point, reason, email);

  // Set the validated gaia id parameter so that the InlineLoginHandler will
  // request a reauth.
  signin_url = net::AppendQueryParameter(
      signin_url, credential_provider::kValidateGaiaIdSigninPromoParameter,
      "gaia_id");

  ui_test_utils::NavigateToURL(browser(), signin_url);
  WaitUntilUIReady(browser());

  // Expected gaia endpoint to load.
  GURL gaia_url = GaiaUrls::GetInstance()->embedded_setup_windows_url();

  EXPECT_TRUE(
      tracker_.PageRequested(gaia_url, {{"flow", "reauth"}, {"email", email}}));
}
#endif
