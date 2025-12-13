// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/cfi_buildflags.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/android_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/identifiers/profile_id_service_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_builder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/policy/policy_ui.h"
#include "chrome/browser/ui/webui/policy/policy_ui_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/account_id/account_id.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/webui/policy_status_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/test_support/embedded_policy_test_server.h"
#include "components/policy/test_support/policy_storage.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/command_line_switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "google_apis/gaia/fake_gaia.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using testing::_;
using testing::Return;

namespace {

constexpr char kTestEmail[] = "enterprise@example.com";
constexpr char kTestRefreshToken[] =
    "test_refresh_token_for_enterprise@example.com";
constexpr char kFakeProfileId[] = "SomeProfileId";

constexpr char kBannerVisible[] = "visible";
constexpr char kBannerHidden[] = "hidden";

constexpr char kValidLocale[] = "en-US";
constexpr char kInvalidLocale[] = "en-GB";

constexpr char kPromotionBannerVisibilityJavaScript[] = R"(
  (function () {
    const element =
      document.getElementsByTagName('promotion-banner-section-container')[0];
    return element ? 'visible' : 'hidden';
  })();
)";

constexpr char kPromotionBannerDismissJavaScript[] = R"(
  const promotionContainer =
    document.getElementsByTagName('promotion-banner-section-container')[0];
  if (promotionContainer){
    const dismissButton =
      promotionContainer.shadowRoot.getElementById('promotion-dismiss-button');
    dismissButton.click();
  }
)";

class PromotionObserver : public PolicyPromotionObserver,
                          public base::test::TestFuture<const std::string&> {
 public:
  void OnPromotionEligibilityFetched(
      const std::string& callback_id,
      enterprise_management::GetUserEligiblePromotionsResponse response)
      override {
    SetValue(callback_id);
  }
};

}  // namespace

// Scoped locale setter to manage the scope of the locale change and ensure the
// locale is set back to the original value after the test is finished.
class ScopedLocaleSetter {
 public:
  explicit ScopedLocaleSetter(std::string_view locale) {
    locale_ = g_browser_process->GetApplicationLocale();
    g_browser_process->SetApplicationLocale(std::string(locale));
  }
  ~ScopedLocaleSetter() { g_browser_process->SetApplicationLocale(locale_); }

 private:
  std::string locale_;
};

class PolicyUIManagedStatusTest : public PlatformBrowserTest,
                                  public ::testing::WithParamInterface<bool> {
 public:
  PolicyUIManagedStatusTest()
      : embedded_test_server_(net::EmbeddedTestServer::TYPE_HTTP) {
    embedded_test_server_.RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, "/oauth2/v1/userinfo",
        base::BindRepeating(&PolicyUIManagedStatusTest::HandleUserInfoRequest,
                            base::Unretained(this))));
    embedded_test_server_.RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, "/devicemanagement",
        base::BindRepeating(
            &PolicyUIManagedStatusTest::HandleDeviceManagementRequest,
            base::Unretained(this))));
    embedded_test_server_.RegisterRequestHandler(base::BindRepeating(
        &FakeGaia::HandleRequest, base::Unretained(&fake_gaia_)));
    scoped_feature_list_.InitWithFeatureState(
        features::kEnablePolicyPromotionBanner, GetParam());
  }
  PolicyUIManagedStatusTest(const PolicyUIManagedStatusTest&) = delete;
  PolicyUIManagedStatusTest& operator=(const PolicyUIManagedStatusTest&) =
      delete;

  ~PolicyUIManagedStatusTest() override = default;

  bool is_feature_enabled() { return GetParam(); }

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server_.InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  // Manually set the static profile ID since some builds do not initialize with
  // it
  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    enterprise::ProfileIdServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          return std::make_unique<enterprise::ProfileIdService>(kFakeProfileId);
        }));
    InProcessBrowserTest::SetUpBrowserContextKeyedServices(context);
  }

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    // Configure policy server
    ASSERT_NO_FATAL_FAILURE(
        policy_server_ = std::make_unique<policy::EmbeddedPolicyTestServer>());
    ASSERT_TRUE(policy_server_->Start());
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    // Configure embedded test server.
    const GURL& base_url = embedded_test_server_.base_url();
    command_line->AppendSwitchASCII(
        policy::switches::kDeviceManagementUrl,
        base_url.Resolve("/devicemanagement").spec());
    command_line->AppendSwitchASCII(switches::kGaiaUrl, base_url.spec());
    command_line->AppendSwitchASCII(switches::kLsoUrl, base_url.spec());
    command_line->AppendSwitchASCII(switches::kGoogleApisUrl, base_url.spec());
    command_line->AppendSwitchASCII(switches::kOAuthAccountManagerUrl,
                                    base_url.spec());
    policy::ChromeBrowserPolicyConnector::EnableCommandLineSupportForTesting();
    fake_gaia_.Initialize();
    // Configure Sync server
    command_line->AppendSwitch(syncer::kDisableSync);
  }

  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();
    embedded_test_server_.StartAcceptingConnections();
    SetupFakeGaiaResponses();
    EnableProfileManagement();
  }

  void EnableProfileManagement() {
    Profile* profile = browser()->profile();
    // ChromeOS creates a client on profile creation, so we only need to setup
    // the registration for it there.
#if BUILDFLAG(IS_CHROMEOS)
    profile->GetCloudPolicyManager()->core()->client()->SetupRegistration(
        "dm_token", "client_id", {});
#else
    auto client = std::make_unique<policy::CloudPolicyClient>(
        /*service=*/g_browser_process->browser_policy_connector()
            ->device_management_service(),
        /*url_loader_factory=*/g_browser_process->shared_url_loader_factory());
    client->SetupRegistration("dm_token", "client_id", {});
    profile->GetCloudPolicyManager()->core()->Connect(std::move(client));
#endif  // !BUILDFLAG(IS_CHROMEOS)
    EXPECT_FALSE(signin::MakeAccountAvailable(
                     IdentityManagerFactory::GetForProfile(profile),
                     signin::AccountAvailabilityOptionsBuilder()
                         .AsPrimary(signin::ConsentLevel::kSignin)
                         .WithRefreshToken(kTestRefreshToken)
                         .Build(kTestEmail))
                     .account_id.empty());
  }

  void SetupFakeGaiaResponses() {
    FakeGaia::AccessTokenInfo access_token_info;
    // Access token is used to represent the types of promotion the user will be
    // qualified for, refer to device_management_backend.proto for source of
    // truth.
    access_token_info.token = "CHROME_ENTERPRISE_CORE";
    access_token_info.any_scope = true;
    access_token_info.audience =
        GaiaUrls::GetInstance()->oauth2_chrome_client_id();
    access_token_info.email = kTestEmail;
    fake_gaia_.IssueOAuthToken(kTestRefreshToken, access_token_info);
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleUserInfoRequest(
      const net::test_server::HttpRequest& r) {
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_content(R"(
        {
          "email": "enterprise@example.com",
          "verified_email": true,
          "hd": "example.com"
        })");
    return http_response;
  }

  // This simply redirects to the policy server.
  std::unique_ptr<net::test_server::HttpResponse> HandleDeviceManagementRequest(
      const net::test_server::HttpRequest& r) {
    std::string request_type;
    net::GetValueForKeyInQuery(r.GetURL(), policy::dm_protocol::kParamRequest,
                               &request_type);

    // Redirect to the policy server.
    GURL::Replacements replace_query;
    std::string query = r.GetURL().GetQuery();
    replace_query.SetQueryStr(query);
    std::string dest =
        policy_server_->GetServiceURL().ReplaceComponents(replace_query).spec();
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
    http_response->AddCustomHeader("Location", dest);
#if BUILDFLAG(IS_CHROMEOS)
    // Intercept policy fetch requests since ChromeOS specifically calls for a
    // policy fetch, and failed policy fetches cause the client dm token to be
    // overridden, furthermore we do not test any policy fetch logic, so we
    // return a basic response.
    if (request_type == "policy") {
      return std::make_unique<net::test_server::BasicHttpResponse>();
    }
#endif  // BUILDFLAG(IS_CHROMEOS)
    return http_response;
  }

 protected:
  void SetPromotionBannerDismissedPref(bool is_dismissed) {
    auto* prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(
        policy::policy_prefs::kHasDismissedPolicyPagePromotionBanner,
        is_dismissed);
  }

  // Helper method to setup and wait for the promotion listener.
  void SetupAndListenForPromotion() {
    auto* handlers = browser()
                         ->tab_strip_model()
                         ->GetActiveWebContents()
                         ->GetWebUI()
                         ->GetHandlersForTesting();

    ASSERT_EQ(handlers->size(), 1u);
    auto* handler = static_cast<PolicyUIHandler*>(handlers[0][0].get());

    // Only wait if the feature is enabled AND locale is en-US AND not
    // dismissed.
    const bool is_dismissed = browser()->profile()->GetPrefs()->GetBoolean(
        policy::policy_prefs::kHasDismissedPolicyPagePromotionBanner);

    if (is_feature_enabled() &&
        g_browser_process->GetApplicationLocale() == kValidLocale &&
        !is_dismissed && !handler->HasPromotionBeenChecked()) {
      // Check if the promotion has already been checked before waiting for the
      // observer to avoid racing condition.
      PromotionObserver promotion_observer;
      handler->AddPolicyPromotionObserver(&promotion_observer);
      EXPECT_TRUE(promotion_observer.Wait());
      handler->RemovePolicyPromotionObserver(&promotion_observer);
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer embedded_test_server_;
  std::unique_ptr<policy::EmbeddedPolicyTestServer> policy_server_;
  FakeGaia fake_gaia_;
};

IN_PROC_BROWSER_TEST_P(PolicyUIManagedStatusTest,
                       HandleGetShowPromotionTestShown) {
  ScopedLocaleSetter locale_setter(kValidLocale);

  SetPromotionBannerDismissedPref(false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIPolicyURL)));
  SetupAndListenForPromotion();

  auto result = EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                       kPromotionBannerVisibilityJavaScript)
                    .ExtractString();

  if (is_feature_enabled()) {
    EXPECT_EQ(result, kBannerVisible);
  } else {
    EXPECT_EQ(result, kBannerHidden);
  }
}

IN_PROC_BROWSER_TEST_P(PolicyUIManagedStatusTest,
                       HandleGetShowPromotionDismisseddHidden) {
  ScopedLocaleSetter locale_setter(kValidLocale);

  SetPromotionBannerDismissedPref(true);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIPolicyURL)));
  SetupAndListenForPromotion();

  auto result = EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                       kPromotionBannerVisibilityJavaScript)
                    .ExtractString();

  EXPECT_EQ(result, kBannerHidden);
}

IN_PROC_BROWSER_TEST_P(PolicyUIManagedStatusTest,
                       HandleSetBannerDismissedHidden) {
  ScopedLocaleSetter locale_setter(kValidLocale);

  SetPromotionBannerDismissedPref(false);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIPolicyURL)));
  SetupAndListenForPromotion();

  EXPECT_TRUE(ExecJs(browser()->tab_strip_model()->GetActiveWebContents(),
                     kPromotionBannerDismissJavaScript));

  auto result = EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                       kPromotionBannerVisibilityJavaScript)
                    .ExtractString();
  EXPECT_EQ(result, kBannerHidden);
}

IN_PROC_BROWSER_TEST_P(PolicyUIManagedStatusTest,
                       HandleLocaleNotEnUSHidden) {
  // The browser's locale needs to be "en-US" to be able to see the banner.
  ScopedLocaleSetter locale_setter(kInvalidLocale);

  SetPromotionBannerDismissedPref(false);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIPolicyURL)));
  SetupAndListenForPromotion();

  auto result = EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                       kPromotionBannerVisibilityJavaScript)
                    .ExtractString();
  EXPECT_EQ(result, kBannerHidden);
}

IN_PROC_BROWSER_TEST_P(PolicyUIManagedStatusTest,
                       HistogramRecordedWhenBannerDisplayed) {
  ScopedLocaleSetter locale_setter(kValidLocale);

  SetPromotionBannerDismissedPref(false);

  base::HistogramTester histogram_tester;

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIPolicyURL)));
  SetupAndListenForPromotion();

  const bool expected_bucket = is_feature_enabled() ? true : false;
  histogram_tester.ExpectBucketCount(
      "Enterprise.PolicyPromotionBannerDisplayed", expected_bucket, 1);
}

IN_PROC_BROWSER_TEST_P(PolicyUIManagedStatusTest, PageLoadedInGuestMode) {
  Browser* policy_browser = OpenURLOffTheRecord(
      browser()->profile(), GURL(chrome::kChromeUIPolicyURL));
  ASSERT_TRUE(policy_browser);
  // In guest mode, the banner should always be hidden, and typically, the
  // promotion eligibility fetch wouldn't even be initiated. So, waiting is not
  // applicable here. We explicitly omit SetupAndListenForPromotion().
  ASSERT_TRUE(ui_test_utils::NavigateToURL(policy_browser,
                                           GURL(chrome::kChromeUIPolicyURL)));

  auto result =
      EvalJs(policy_browser->tab_strip_model()->GetActiveWebContents(),
             kPromotionBannerVisibilityJavaScript)
          .ExtractString();
  EXPECT_EQ(result, kBannerHidden);
}

INSTANTIATE_TEST_SUITE_P(PolicyManagedUITestInstance,
                         PolicyUIManagedStatusTest,
                         ::testing::Values(false, true));
