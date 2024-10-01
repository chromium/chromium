// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time_override.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_icon_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/content_settings/core/common/tracking_protection_feature.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/views/widget/any_widget_observer.h"
#include "url/gurl.h"

using Status = content_settings::TrackingProtectionBlockingStatus;
using FeatureType = content_settings::TrackingProtectionFeatureType;

class CookieControlsBubbleViewPixelTest
    : public DialogBrowserTest,
      public testing::WithParamInterface<CookieBlocking3pcdStatus> {
 public:
  CookieControlsBubbleViewPixelTest() = default;

  void TearDownOnMainThread() override {
    cookie_controls_coordinator_ = nullptr;
    cookie_controls_icon_ = nullptr;
    DialogBrowserTest::TearDownOnMainThread();
  }

  CookieControlsBubbleViewPixelTest(const CookieControlsBubbleViewPixelTest&) =
      delete;
  CookieControlsBubbleViewPixelTest& operator=(
      const CookieControlsBubbleViewPixelTest&) = delete;

  void SetUp() override {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::test_server::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_->AddDefaultHandlers(base::FilePath(GetChromeTestDataDir()));
    DialogBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DialogBrowserTest::SetUpCommandLine(command_line);
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
    DialogBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    DialogBrowserTest::TearDownInProcessBrowserTestFixture();
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    // This test uses a mock time, so use mock cert verifier to not have cert
    // verification depend on the current mocked time.
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);

    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(https_test_server());
    ASSERT_TRUE(https_test_server()->Start());

    cookie_controls_icon_ = static_cast<CookieControlsIconView*>(
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar_button_provider()
            ->GetPageActionIconView(PageActionIconType::kCookieControls));
    ASSERT_TRUE(cookie_controls_icon_);

    controller_ = std::make_unique<content_settings::CookieControlsController>(
        CookieSettingsFactory::GetForProfile(browser()->profile()),
        /*original_cookie_settings=*/nullptr,
        HostContentSettingsMapFactory::GetForProfile(browser()->profile()),
        /*tracking_protection_settings=*/nullptr);

    cookie_controls_coordinator_ =
        cookie_controls_icon_->GetCoordinatorForTesting();
    cookie_controls_coordinator_->SetDisplayNameForTesting(u"example.com");
  }

  void SetStatus(
      bool controls_visible,
      bool protections_on,
      CookieControlsEnforcement enforcement,
      CookieBlocking3pcdStatus blocking_status,
      int days_to_expiration,
      std::vector<content_settings::TrackingProtectionFeature> features) {
    // ShowBubble will initialize the view controller.
    cookie_controls_coordinator_->ShowBubble(
        browser()->tab_strip_model()->GetActiveWebContents(),
        controller_.get());
    auto expiration = days_to_expiration
                          ? base::Time::Now() + base::Days(days_to_expiration)
                          : base::Time();
    // TODO: 344042974 - This should be updated to set directly on
    // CookieControlsController. Currently if the page action icon is updated
    // after OnStatusChanged() is called it will pull state from
    // CookieControlsController, which has not been updated to reflect what is
    // needed for this test.
    view_controller()->OnStatusChanged(controls_visible, protections_on,
                                       enforcement, blocking_status, expiration,
                                       features);
    cookie_controls_icon()->DisableUpdatesForTesting();
  }

  static base::Time GetReferenceTime() {
    base::Time time;
    EXPECT_TRUE(base::Time::FromString("Sat, 1 Sep 2023 11:00:00 UTC", &time));
    return time;
  }

  void NavigateToUrlWithThirdPartyCookies() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::CookieChangeObserver observer(web_contents);

    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), third_party_cookie_page_url()));
    observer.Wait();
  }

  void BlockThirdPartyCookies() {
    bool pre_3pcd = GetParam() == CookieBlocking3pcdStatus::kNotIn3pcd;
    if (pre_3pcd) {
      browser()->profile()->GetPrefs()->SetInteger(
          prefs::kCookieControlsMode,
          static_cast<int>(
              content_settings::CookieControlsMode::kBlockThirdParty));
    } else {
      browser()->profile()->GetPrefs()->SetBoolean(
          prefs::kTrackingProtection3pcdEnabled, true);
    }
  }

  std::vector<content_settings::TrackingProtectionFeature>
  GetTrackingProtectionFeatures() {
    if (protections_on_) {
      if (GetParam() == CookieBlocking3pcdStatus::kLimited) {
        return {
            {FeatureType::kThirdPartyCookies, enforcement_, Status::kLimited}};
      } else {
        return {
            {FeatureType::kThirdPartyCookies, enforcement_, Status::kBlocked}};
      }
    }
    return {{FeatureType::kThirdPartyCookies, enforcement_, Status::kAllowed}};
  }

  void ShowUi(const std::string& name_with_param_suffix) override {
    BlockThirdPartyCookies();
    NavigateToUrlWithThirdPartyCookies();
    ASSERT_TRUE(cookie_controls_icon()->GetVisible());
    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "CookieControlsBubbleViewImpl");
    cookie_controls_icon()->ExecuteForTesting();
    SetStatus(controls_visible_, protections_on_, enforcement_, GetParam(),
              days_to_expiration_, GetTrackingProtectionFeatures());
    waiter.WaitIfNeededAndGet();

    // Even with the waiter, it's possible that the toggle is in the process
    // of animating into the appropriate position. Include a small delay here
    // to let that animation complete.
    base::RunLoop run_loop;
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(500));
    run_loop.Run();
  }

  scoped_refptr<content_settings::CookieSettings> cookie_settings() {
    return CookieSettingsFactory::GetForProfile(browser()->profile());
  }
  HostContentSettingsMap* host_content_settings_map() {
    return HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  }
  GURL third_party_cookie_page_url() {
    return https_test_server()->GetURL("a.test",
                                       "/third_party_partitioned_cookies.html");
  }

  CookieControlsIconView* cookie_controls_icon() {
    return cookie_controls_icon_;
  }
  net::EmbeddedTestServer* https_test_server() { return https_server_.get(); }

  CookieControlsBubbleViewController* view_controller() {
    return cookie_controls_coordinator_->GetViewControllerForTesting();
  }

 protected:
  bool controls_visible_ = true;
  bool protections_on_ = true;
  CookieControlsEnforcement enforcement_ =
      CookieControlsEnforcement::kNoEnforcement;
  int days_to_expiration_ = 0;
  // Overriding `base::Time::Now()` to obtain a consistent X days until
  // exception expiration calculation regardless of the time the test runs.
  base::subtle::ScopedTimeClockOverrides time_override_{
      &CookieControlsBubbleViewPixelTest::GetReferenceTime,
      /*time_ticks_override=*/nullptr, /*thread_ticks_override=*/nullptr};
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  content::ContentMockCertVerifier mock_cert_verifier_;
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<CookieControlsIconView> cookie_controls_icon_;
  std::unique_ptr<content_settings::CookieControlsController> controller_;
  raw_ptr<CookieControlsBubbleCoordinator> cookie_controls_coordinator_;
};

IN_PROC_BROWSER_TEST_P(CookieControlsBubbleViewPixelTest,
                       InvokeUi_CookiesBlocked) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(CookieControlsBubbleViewPixelTest,
                       InvokeUi_PermanentException) {
  protections_on_ = false;
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(CookieControlsBubbleViewPixelTest,
                       InvokeUi_TemporaryException) {
  protections_on_ = false;
  days_to_expiration_ = 90;
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(CookieControlsBubbleViewPixelTest,
                       InvokeUi_EnforcedByCookieSetting) {
  protections_on_ = false;
  enforcement_ = CookieControlsEnforcement::kEnforcedByCookieSetting;
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(CookieControlsBubbleViewPixelTest,
                       InvokeUi_EnforcedByPolicy) {
  protections_on_ = false;
  enforcement_ = CookieControlsEnforcement::kEnforcedByPolicy;
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(CookieControlsBubbleViewPixelTest,
                       InvokeUi_EnforcedByExtension) {
  protections_on_ = false;
  enforcement_ = CookieControlsEnforcement::kEnforcedByExtension;
  ShowAndVerifyUi();
}

std::string ParamToTestSuffix(
    const testing::TestParamInfo<CookieControlsBubbleViewPixelTest::ParamType>&
        info) {
  std::stringstream name;
  name << "3pcd";
  switch (info.param) {
    case CookieBlocking3pcdStatus::kNotIn3pcd:
      name << "Off";
      break;
    case CookieBlocking3pcdStatus::kLimited:
      name << "Limited";
      break;
    case CookieBlocking3pcdStatus::kAll:
      name << "BlockAll";
      break;
  }
  return name.str();
}

INSTANTIATE_TEST_SUITE_P(
    /*no prefix*/,
    CookieControlsBubbleViewPixelTest,
    testing::ValuesIn({CookieBlocking3pcdStatus::kNotIn3pcd,
                       CookieBlocking3pcdStatus::kLimited,
                       CookieBlocking3pcdStatus::kAll}),
    &ParamToTestSuffix);

// TODO(https://b/354946320): Add pixel tests for ACT feature states once we
// have UX.
