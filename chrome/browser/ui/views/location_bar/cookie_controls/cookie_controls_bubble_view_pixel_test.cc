// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/dips/dips_service.h"
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
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/views/widget/any_widget_observer.h"
#include "url/gurl.h"

namespace {

void ApplySetting(content_settings::CookieSettings* cookie_settings,
                  HostContentSettingsMap* hcsm,
                  const GURL& url,
                  bool cookies_enabled,
                  CookieControlsEnforcement enforcement) {
  switch (enforcement) {
    case (CookieControlsEnforcement::kNoEnforcement): {
      if (cookies_enabled) {
        cookie_settings->SetCookieSettingForUserBypass(url);
      }
      return;
    }
    case (CookieControlsEnforcement::kEnforcedByPolicy): {
      auto provider = std::make_unique<content_settings::MockProvider>();
      provider->SetWebsiteSetting(
          ContentSettingsPattern::Wildcard(),
          ContentSettingsPattern::FromURL(url), ContentSettingsType::COOKIES,
          base::Value(cookies_enabled ? ContentSetting::CONTENT_SETTING_ALLOW
                                      : ContentSetting::CONTENT_SETTING_BLOCK));
      content_settings::TestUtils::OverrideProvider(
          hcsm, std::move(provider), HostContentSettingsMap::POLICY_PROVIDER);
      return;
    }
    case (CookieControlsEnforcement::kEnforcedByCookieSetting):
      hcsm->SetContentSettingCustomScope(
          ContentSettingsPattern::Wildcard(),
          ContentSettingsPattern::FromString("[*.]test"),
          ContentSettingsType::COOKIES,
          cookies_enabled ? ContentSetting::CONTENT_SETTING_ALLOW
                          : ContentSetting::CONTENT_SETTING_BLOCK);
      return;
    case (CookieControlsEnforcement::kEnforcedByExtension):
      auto provider = std::make_unique<content_settings::MockProvider>();
      provider->SetWebsiteSetting(
          ContentSettingsPattern::Wildcard(),
          ContentSettingsPattern::FromURL(url), ContentSettingsType::COOKIES,
          base::Value(cookies_enabled ? ContentSetting::CONTENT_SETTING_ALLOW
                                      : ContentSetting::CONTENT_SETTING_BLOCK));
      content_settings::TestUtils::OverrideProvider(
          hcsm, std::move(provider),
          HostContentSettingsMap::CUSTOM_EXTENSION_PROVIDER);
      return;
  }
}

}  // namespace

class CookieControlsBubbleViewPixelTest
    : public DialogBrowserTest,
      public testing::WithParamInterface<
          std::tuple<bool, std::string, CookieControlsEnforcement>> {
 public:
  CookieControlsBubbleViewPixelTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        content_settings::features::kUserBypassUI,
        {{"expiration", std::get<1>(GetParam())}});
  }

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

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(https_test_server());
    ASSERT_TRUE(https_test_server()->Start());

    cookie_controls_icon_ = static_cast<CookieControlsIconView*>(
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar_button_provider()
            ->GetPageActionIconView(PageActionIconType::kCookieControls));
    ASSERT_TRUE(cookie_controls_icon_);
    cookie_controls_coordinator_ =
        cookie_controls_icon_->GetCoordinatorForTesting();
    cookie_controls_coordinator_->SetDisplayNameForTesting(u"example.com");
  }

  void ShowUi(const std::string& name) override {
    auto cookies_enabled = std::get<0>(GetParam());
    auto exception_duration = std::get<1>(GetParam());
    auto enforcement = std::get<2>(GetParam());
    SetThirdPartyCookieBlocking(true);

    // Name is not considered when determining test state.
    ApplySetting(cookie_settings().get(), host_content_settings_map(),
                 third_party_cookie_page_url(), cookies_enabled, enforcement);

    NavigateToUrlWithThirdPartyCookies();

    ASSERT_TRUE(cookie_controls_icon()->GetVisible());
    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "CookieControlsBubbleViewImpl");

    cookie_controls_icon()->ExecuteForTesting();
    waiter.WaitIfNeededAndGet();

    // Even with the waiter, it's possible that the toggle is in the process
    // of animating into the appropriate position. Include a small delay here
    // to let that animation complete.
    base::RunLoop run_loop;
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(500));
    run_loop.Run();
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

  void SetThirdPartyCookieBlocking(bool enabled) {
    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(
            enabled ? content_settings::CookieControlsMode::kBlockThirdParty
                    : content_settings::CookieControlsMode::kOff));
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

  PageActionIconView* cookie_controls_icon() { return cookie_controls_icon_; }
  net::EmbeddedTestServer* https_test_server() { return https_server_.get(); }

 private:
  // Overriding `base::Time::Now()` to obtain a consistent X days until
  // exception expiration calculation regardless of the time the test runs.
  base::subtle::ScopedTimeClockOverrides time_override_{
      &CookieControlsBubbleViewPixelTest::GetReferenceTime,
      /*time_ticks_override=*/nullptr, /*thread_ticks_override=*/nullptr};
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<CookieControlsIconView> cookie_controls_icon_;
  raw_ptr<CookieControlsBubbleCoordinator> cookie_controls_coordinator_;
};

IN_PROC_BROWSER_TEST_P(CookieControlsBubbleViewPixelTest, InvokeUi) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(
    /*no prefix*/,
    CookieControlsBubbleViewPixelTest,
    testing::Combine(
        testing::Bool(),
        testing::ValuesIn({std::string("0d"), std::string("30d"),
                           std::string("90d")}),
        testing::ValuesIn(
            {CookieControlsEnforcement::kNoEnforcement,
             CookieControlsEnforcement::kEnforcedByPolicy,
             CookieControlsEnforcement::kEnforcedByExtension,
             CookieControlsEnforcement::kEnforcedByCookieSetting})),
    [](const testing::TestParamInfo<
        CookieControlsBubbleViewPixelTest::ParamType>& info) {
      std::stringstream name;
      name << "cookies_enabled_"
           << (std::get<0>(info.param) ? "true" : "false");
      name << "_exception_duration_" << std::get<1>(info.param);
      name << "_enforcement_";
      switch (std::get<2>(info.param)) {
        case (CookieControlsEnforcement::kNoEnforcement):
          name << "none";
          break;
        case (CookieControlsEnforcement::kEnforcedByPolicy):
          name << "policy";
          break;
        case (CookieControlsEnforcement::kEnforcedByExtension):
          name << "extension";
          break;
        case (CookieControlsEnforcement::kEnforcedByCookieSetting):
          name << "settings";
          break;
      }
      return name.str();
    });
