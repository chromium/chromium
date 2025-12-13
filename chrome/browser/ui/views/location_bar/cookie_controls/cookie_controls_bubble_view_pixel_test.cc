// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time_override.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_coordinator.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_view_controller.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_icon_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/cookie_blocking_3pcd_status.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"
#include "components/content_settings/core/common/cookie_controls_state.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/views/widget/any_widget_observer.h"
#include "url/gurl.h"

class CookieControlsBubbleViewPixelTestBase : public DialogBrowserTest {
 public:
  CookieControlsBubbleViewPixelTestBase() = default;

  void TearDownOnMainThread() override {
    cookie_controls_coordinator_ = nullptr;
    controller_ = nullptr;
    incognito_controller_ = nullptr;
    cookie_controls_icon_ = nullptr;
    DialogBrowserTest::TearDownOnMainThread();
  }

  CookieControlsBubbleViewPixelTestBase(
      const CookieControlsBubbleViewPixelTestBase&) = delete;
  CookieControlsBubbleViewPixelTestBase& operator=(
      const CookieControlsBubbleViewPixelTestBase&) = delete;

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

    cookie_controls_icon_ = BrowserView::GetBrowserViewForBrowser(browser())
                                ->toolbar_button_provider()
                                ->GetPageActionView(kActionShowCookieControls);
    ASSERT_TRUE(cookie_controls_icon_);

    controller_ = std::make_unique<content_settings::CookieControlsController>(
        CookieSettingsFactory::GetForProfile(browser()->profile()),
        /*original_cookie_settings=*/nullptr,
        HostContentSettingsMapFactory::GetForProfile(browser()->profile()),
        TrackingProtectionSettingsFactory::GetForProfile(browser()->profile()),
        /*is_incognito_profile=*/false);

    incognito_controller_ =
        std::make_unique<content_settings::CookieControlsController>(
            CookieSettingsFactory::GetForProfile(incognito_profile()),
            CookieSettingsFactory::GetForProfile(browser()->profile()),
            HostContentSettingsMapFactory::GetForProfile(incognito_profile()),
            TrackingProtectionSettingsFactory::GetForProfile(
                incognito_profile()),
            /*is_incognito_profile=*/true);

    cookie_controls_coordinator_ =
        CookieControlsBubbleCoordinator::From(browser());
    cookie_controls_coordinator_->SetDisplayNameForTesting(u"example.com");
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

  IconLabelBubbleView* cookie_controls_icon() { return cookie_controls_icon_; }
  net::EmbeddedTestServer* https_test_server() { return https_server_.get(); }

  CookieControlsBubbleViewController* view_controller() {
    return cookie_controls_coordinator_->GetViewControllerForTesting();
  }

  Profile* incognito_profile() {
    return browser()->profile()->GetPrimaryOTRProfile(true);
  }

 protected:
  CookieControlsEnforcement enforcement_ =
      CookieControlsEnforcement::kNoEnforcement;
  CookieControlsState controls_state_ = CookieControlsState::kBlocked3pc;

  int days_to_expiration_ = 0;
  // Overriding `base::Time::Now()` to obtain a consistent X days until
  // exception expiration calculation regardless of the time the test runs.
  base::subtle::ScopedTimeClockOverrides time_override_{
      &CookieControlsBubbleViewPixelTestBase::GetReferenceTime,
      /*time_ticks_override=*/nullptr, /*thread_ticks_override=*/nullptr};
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  content::ContentMockCertVerifier mock_cert_verifier_;
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<IconLabelBubbleView> cookie_controls_icon_;
  std::unique_ptr<content_settings::CookieControlsController> controller_;
  std::unique_ptr<content_settings::CookieControlsController>
      incognito_controller_;
  raw_ptr<CookieControlsBubbleCoordinator> cookie_controls_coordinator_;
};

class CookieControlsBubbleViewPixelTest
    : public CookieControlsBubbleViewPixelTestBase,
      public testing::WithParamInterface<CookieBlocking3pcdStatus> {
 public:
  CookieControlsBubbleViewPixelTest() = default;
  CookieControlsBubbleViewPixelTest(const CookieControlsBubbleViewPixelTest&) =
      delete;
  CookieControlsBubbleViewPixelTest& operator=(
      const CookieControlsBubbleViewPixelTest&) = delete;

  void SetUp() override {
    if (GetParam() != CookieBlocking3pcdStatus::kNotIn3pcd) {
      scoped_feature_list_.InitAndEnableFeature(
          content_settings::features::kTrackingProtection3pcd);
    }
    CookieControlsBubbleViewPixelTestBase::SetUp();
  }

  void BlockThirdPartyCookies() {
    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(
            content_settings::CookieControlsMode::kBlockThirdParty));
  }

  void SetStatus(CookieControlsState controls_state,
                 CookieControlsEnforcement enforcement,
                 CookieBlocking3pcdStatus blocking_status,
                 int days_to_expiration) {
    // ShowBubble will initialize the view controller.
    cookie_controls_coordinator_->ShowBubble(
        browser()->GetBrowserView().toolbar_button_provider(),
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
    view_controller()->OnStatusChanged(controls_state, enforcement,
                                       blocking_status, expiration);
    if (!IsPageActionMigrated(PageActionIconType::kCookieControls)) {
      static_cast<CookieControlsIconView*>(cookie_controls_icon())
          ->ExecuteForTesting();
    }
  }

  void ShowUi(const std::string& name_with_param_suffix) override {
    BlockThirdPartyCookies();
    NavigateToUrlWithThirdPartyCookies();
    ASSERT_TRUE(cookie_controls_icon()->GetVisible());
    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "CookieControlsBubbleViewImpl");
    if (IsPageActionMigrated(PageActionIconType::kCookieControls)) {
      actions::ActionManager::Get()
          .FindAction(kActionShowCookieControls)
          ->InvokeAction();
    } else {
      static_cast<CookieControlsIconView*>(cookie_controls_icon())
          ->ExecuteForTesting();
    }

    SetStatus(controls_state_, enforcement_, GetParam(), days_to_expiration_);
    waiter.WaitIfNeededAndGet();

    // Even with the waiter, it's possible that the toggle is in the process
    // of animating into the appropriate position. Include a small delay here
    // to let that animation complete.
    base::RunLoop run_loop;
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(500));
    run_loop.Run();
  }
};

IN_PROC_BROWSER_TEST_P(CookieControlsBubbleViewPixelTest,
                       InvokeUi_CookiesBlocked) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(CookieControlsBubbleViewPixelTest,
                       InvokeUi_PermanentException) {
  set_baseline("6229914");
  controls_state_ = CookieControlsState::kAllowed3pc;
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(CookieControlsBubbleViewPixelTest,
                       InvokeUi_TemporaryException) {
  set_baseline("6229914");
  controls_state_ = CookieControlsState::kAllowed3pc;
  days_to_expiration_ = 90;
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(CookieControlsBubbleViewPixelTest,
                       InvokeUi_EnforcedByCookieSetting) {
  controls_state_ = CookieControlsState::kAllowed3pc;
  enforcement_ = CookieControlsEnforcement::kEnforcedByCookieSetting;
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(CookieControlsBubbleViewPixelTest,
                       InvokeUi_EnforcedByPolicy) {
  controls_state_ = CookieControlsState::kAllowed3pc;
  enforcement_ = CookieControlsEnforcement::kEnforcedByPolicy;
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(CookieControlsBubbleViewPixelTest,
                       InvokeUi_EnforcedByExtension) {
  controls_state_ = CookieControlsState::kAllowed3pc;
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
