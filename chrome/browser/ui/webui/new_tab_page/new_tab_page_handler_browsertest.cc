// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_handler.h"

#include <memory>

#include "base/metrics/statistics_recorder.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/new_tab_page/feature_promo_helper/new_tab_page_feature_promo_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/browser/search_provider_logos/logo_service_factory.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/customize_chrome/side_panel_controller.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/webui/customize_buttons/customize_buttons_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#endif

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTabPageElementId);
const WebContentsInteractionTestUtil::DeepQuery kCustomizeChromeIPH{
    "ntp-app", "ntp-customize-buttons", "help-bubble"};
}  // namespace

class MockPage : public new_tab_page::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<new_tab_page::mojom::Page> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  MOCK_METHOD(void, SetTheme, (new_tab_page::mojom::ThemePtr));
  MOCK_METHOD(void,
              SetDisabledModules,
              (bool, const std::vector<std::string>&));
  MOCK_METHOD(void, SetModulesLoadable, ());
  MOCK_METHOD(void, SetActionChipsVisibility, (bool));
  MOCK_METHOD(void, SetModulesFreVisibility, (bool));
  MOCK_METHOD(void, SetCustomizeChromeSidePanelVisibility, (bool));
  MOCK_METHOD(void, ShowWebstoreToast, ());
  MOCK_METHOD(void, SetWallpaperSearchButtonVisibility, (bool));
  MOCK_METHOD(void, FooterVisibilityUpdated, (bool));
  MOCK_METHOD(void,
              ConnectToParentDocument,
              (mojo::PendingRemote<
                  new_tab_page::mojom::MicrosoftAuthUntrustedDocument>));

  mojo::Receiver<new_tab_page::mojom::Page> receiver_{this};
};

class NewTabPageHandlerBaseBrowserTest : public InProcessBrowserTest {
 public:
  void TearDownOnMainThread() override {
    handler_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  Profile* profile() { return browser()->GetProfile(); }
  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  MockPage* mock_page() { return &mock_page_; }
  NewTabPageHandler& handler() { return *handler_; }

 protected:
  void CreateHandlerAndVerifyExpectations() {
    handler_ = std::make_unique<NewTabPageHandler>(
        mojo::PendingReceiver<new_tab_page::mojom::PageHandler>(),
        mock_page_.BindAndGetRemote(), profile(),
        NtpCustomBackgroundServiceFactory::GetForProfile(profile()),
        ThemeServiceFactory::GetForProfile(profile()),
        LogoServiceFactory::GetForProfile(profile()),
        /*sync_service=*/nullptr,
        /*segmentation_platform_service=*/nullptr, web_contents(),
        /*ntp_navigation_start_time=*/base::Time::Now(),
        /*ntp_navigation_start_time_ticks=*/base::TimeTicks::Now(),
        /*module_id_details=*/nullptr);
    testing::Mock::VerifyAndClearExpectations(mock_page());
  }

 private:
  std::unique_ptr<NewTabPageHandler> handler_;
  testing::NiceMock<MockPage> mock_page_;
};

class NewTabPageHandlerCustomizeChromeAutoOpenBrowserTest
    : public InteractiveFeaturePromoTestMixin<
          NewTabPageHandlerBaseBrowserTest> {
 public:
  NewTabPageHandlerCustomizeChromeAutoOpenBrowserTest()
      : InteractiveFeaturePromoTestMixin(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHDesktopCustomizeChromeAutoOpenFeature})) {}

 protected:
  bool IsCustomizeChromeEntryShowing() {
    auto* controller = customize_chrome::SidePanelController::Get(
        chrome_test_utils::GetActiveTab(this)->GetUnownedUserDataHost());
    return controller->IsCustomizeChromeEntryShowing();
  }

  void OpenNewTabPageInForeground() {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), chrome::ChromeUINewTabPageURLAsGURL(),
        WindowOpenDisposition::kNewForegroundTab,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      ntp_features::kNtpCustomizeChromeAutoOpen};
};

IN_PROC_BROWSER_TEST_F(NewTabPageHandlerCustomizeChromeAutoOpenBrowserTest,
                       ShouldAutoOpenSidePanelWhenEligible) {
  OpenNewTabPageInForeground();
  RunTestSequence(
      InstrumentTab(kNewTabPageElementId),
      InAnyContext(WaitForShow(
          CustomizeButtonsHandler::kCustomizeChromeButtonElementId)),
      WaitForElementVisible(kNewTabPageElementId, kCustomizeChromeIPH),
      CheckPromoRequested(
          feature_engagement::kIPHDesktopCustomizeChromeAutoOpenFeature),
      WaitForShow(kSidePanelElementId));

  EXPECT_TRUE(IsCustomizeChromeEntryShowing());
  EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                prefs::kNtpCustomizeChromeSidePanelAutoOpeningsCount),
            1);
  histogram_tester_.ExpectUniqueSample(
      "NewTabPage.CustomizeChromePromoEligibility",
      NTPCustomizeChromePromoEligibility::kCanShowPromo, 1);
  histogram_tester_.ExpectUniqueSample(
      "SidePanel.OpenTrigger",
      SidePanelOpenTrigger::kNewTabPageAutomaticCustomizeChrome, 1);
}

IN_PROC_BROWSER_TEST_F(NewTabPageHandlerCustomizeChromeAutoOpenBrowserTest,
                       ShouldNotAutoOpenSidePanelWhenNotEligible) {
  profile()->GetPrefs()->SetBoolean(prefs::kNtpCustomizeChromeExplicitlyClosed,
                                    true);

  base::StatisticsRecorder::HistogramWaiter promo_eligibility_waiter(
      "NewTabPage.CustomizeChromePromoEligibility");
  OpenNewTabPageInForeground();
  RunTestSequence(InAnyContext(
      WaitForShow(CustomizeButtonsHandler::kCustomizeChromeButtonElementId)));
  promo_eligibility_waiter.Wait();
  RunTestSequence(CheckPromoRequested(
      feature_engagement::kIPHDesktopCustomizeChromeAutoOpenFeature, false));

  EXPECT_FALSE(IsCustomizeChromeEntryShowing());
  EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                prefs::kNtpCustomizeChromeSidePanelAutoOpeningsCount),
            0);
  histogram_tester_.ExpectUniqueSample(
      "NewTabPage.CustomizeChromePromoEligibility",
      NTPCustomizeChromePromoEligibility::
          kCustomizeChromeClosedExplicitlyByUser,
      1);
  histogram_tester_.ExpectTotalCount("SidePanel.OpenTrigger", 0);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
class NewTabPageHandlerManagedTest : public NewTabPageHandlerBaseBrowserTest,
                                     public testing::WithParamInterface<bool> {
 public:
  NewTabPageHandlerManagedTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{ntp_features::kNtpFooter},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    NewTabPageHandlerBaseBrowserTest::SetUpOnMainThread();

    CreateHandlerAndVerifyExpectations();

    // Simulate browser management.
    scoped_browser_management_ =
        std::make_unique<policy::ScopedManagementServiceOverrideForTesting>(
            policy::ManagementServiceFactory::GetForProfile(profile()),
            managed() ? policy::EnterpriseManagementAuthority::DOMAIN_LOCAL
                      : policy::EnterpriseManagementAuthority::NONE);

    NavigateToNewTabPage();
    mock_page()->FlushForTesting();
  }

  void TearDownOnMainThread() override {
    scoped_browser_management_.reset();
    NewTabPageHandlerBaseBrowserTest::TearDownOnMainThread();
  }

  void NavigateToNewTabPage() {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), chrome::ChromeUINewTabPageURLAsGURL(),
        WindowOpenDisposition::kCurrentTab,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  bool managed() { return GetParam(); }
  PrefService* local_state() { return g_browser_process->local_state(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<policy::ScopedManagementServiceOverrideForTesting>
      scoped_browser_management_;
};

INSTANTIATE_TEST_SUITE_P(, NewTabPageHandlerManagedTest, testing::Bool());

IN_PROC_BROWSER_TEST_P(NewTabPageHandlerManagedTest, UpdateFooterVisibility) {
  EXPECT_CALL(*mock_page(), FooterVisibilityUpdated)
      .WillOnce([this](bool visible) { EXPECT_EQ(managed(), visible); });

  handler().UpdateFooterVisibility();
  mock_page()->FlushForTesting();
}

IN_PROC_BROWSER_TEST_P(NewTabPageHandlerManagedTest, SetNoticePolicyPref) {
  bool visible;
  EXPECT_CALL(*mock_page(), FooterVisibilityUpdated)
      .Times(2)
      .WillRepeatedly([&visible](bool visible_arg) { visible = visible_arg; });

  local_state()->SetBoolean(prefs::kNTPFooterManagementNoticeEnabled, false);
  mock_page()->FlushForTesting();

  EXPECT_FALSE(visible);

  local_state()->SetBoolean(prefs::kNTPFooterManagementNoticeEnabled, true);
  mock_page()->FlushForTesting();

  EXPECT_EQ(managed(), visible);
}

// Verifies footer visibility respects user preference when no enterprise label
// is set.
IN_PROC_BROWSER_TEST_P(NewTabPageHandlerManagedTest, SetCustomFooterLabel) {
  bool visible;
  EXPECT_CALL(*mock_page(), FooterVisibilityUpdated)
      .Times(3)
      .WillRepeatedly([&visible](bool visible_arg) { visible = visible_arg; });

  profile()->GetPrefs()->SetBoolean(prefs::kNtpFooterVisible, false);
  mock_page()->FlushForTesting();

  EXPECT_FALSE(visible);

  local_state()->SetString(prefs::kEnterpriseCustomLabelForBrowser,
                           "CustomLabel");
  mock_page()->FlushForTesting();

  EXPECT_EQ(managed(), visible);

  local_state()->SetString(prefs::kEnterpriseCustomLabelForBrowser, "");
  mock_page()->FlushForTesting();

  EXPECT_FALSE(visible);
}

// Verifies footer visibility respects user preference when no enterprise logo
// is set.
IN_PROC_BROWSER_TEST_P(NewTabPageHandlerManagedTest, SetCustomFooterLogo) {
  bool visible;
  EXPECT_CALL(*mock_page(), FooterVisibilityUpdated)
      .Times(3)
      .WillRepeatedly([&visible](bool visible_arg) { visible = visible_arg; });

  profile()->GetPrefs()->SetBoolean(prefs::kNtpFooterVisible, false);
  mock_page()->FlushForTesting();

  EXPECT_FALSE(visible);

  local_state()->SetString(prefs::kEnterpriseLogoUrlForBrowser, "logo_url");
  mock_page()->FlushForTesting();

  EXPECT_EQ(managed(), visible);

  local_state()->SetString(prefs::kEnterpriseLogoUrlForBrowser, "");
  mock_page()->FlushForTesting();

  EXPECT_FALSE(visible);
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
