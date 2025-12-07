// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_handler.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/new_tab_page/feature_promo_helper/new_tab_page_feature_promo_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/browser/search_provider_logos/logo_service_factory.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/customize_chrome/side_panel_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/webui/customize_buttons/customize_buttons_handler.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
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
  MOCK_METHOD(void, SetPromo, (new_tab_page::mojom::PromoPtr));
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

  Profile* profile() { return browser()->profile(); }
  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void CloseSidePanel() {
    BrowserWindowInterface* const browser_window_interface =
        webui::GetBrowserWindowInterface(web_contents());
    SidePanelRegistry* const side_panel_registry =
        browser_window_interface->GetTabStripModel()
            ->GetActiveTab()
            ->GetTabFeatures()
            ->side_panel_registry();
    SidePanelEntry::PanelType panel_type =
        side_panel_registry
            ->GetEntryForKey(
                SidePanelEntryKey(SidePanelEntryId::kCustomizeChrome))
            ->type();
    browser_window_interface->GetFeatures().side_panel_ui()->Close(panel_type);
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
        /*module_id_details=*/nullptr);
    testing::Mock::VerifyAndClearExpectations(mock_page());
  }

 private:
  std::unique_ptr<NewTabPageHandler> handler_;
  testing::NiceMock<MockPage> mock_page_;
};

// TODO(crbug.com/454014654): Check which tests can be ran in
// new_tab_page_handler_unittest.cc and move them there.
class NewTabPageHandlerWithCustomizeChromePromoBaseBrowserTest
    : public NewTabPageHandlerBaseBrowserTest {
 protected:
  bool IsCustomizeChromeEntryShowing() {
    return webui::GetBrowserWindowInterface(web_contents())
        ->GetTabStripModel()
        ->GetActiveTab()
        ->GetTabFeatures()
        ->customize_chrome_side_panel_controller()
        ->IsCustomizeChromeEntryShowing();
  }

  void OpenNewTabPageInForeground() {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(chrome::kChromeUINewTabPageURL),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      ntp_features::kNtpCustomizeChromeAutoOpen};
};

// This class skips the IPH part, please test them in
// NewTabPageHandlerWithCustomizeChromePromoIPHOnlyBrowserTest.
class NewTabPageHandlerWithCustomizeChromePromoBrowserTest
    : public InteractiveBrowserTestMixin<
          NewTabPageHandlerWithCustomizeChromePromoBaseBrowserTest> {
 protected:
  void SetUpOnMainThread() override {
    InteractiveBrowserTestMixin<
        NewTabPageHandlerWithCustomizeChromePromoBaseBrowserTest>::
        SetUpOnMainThread();
    profile()->GetPrefs()->SetBoolean(prefs::kNtpCustomizeChromeIPHAutoOpened,
                                      true);
  }

  void OpenNewTabPageInForegroundAndWaitForLoad() {
    OpenNewTabPageInForeground();
    RunTestSequence(InAnyContext(
        WaitForShow(CustomizeButtonsHandler::kCustomizeChromeButtonElementId)));
  }
};

IN_PROC_BROWSER_TEST_F(NewTabPageHandlerWithCustomizeChromePromoBrowserTest,
                       DontOpenPanelWhenUserCustomizedChromeAlready) {
  auto* theme_service = ThemeServiceFactory::GetForProfile(profile());
  theme_service->SetUserColorAndBrowserColorVariant(
      SkColorSetRGB(0x00, 0x00, 0x00),
      ui::mojom::BrowserColorVariant::kVibrant);

  OpenNewTabPageInForegroundAndWaitForLoad();
  EXPECT_FALSE(IsCustomizeChromeEntryShowing());

  histogram_tester_.ExpectUniqueSample(
      "NewTabPage.CustomizeChromePromoEligibility",
      NTPCustomizeChromePromoEligibility::kChromeCustomizedAlready, 1);
  histogram_tester_.ExpectUniqueSample(
      "SidePanel.OpenTrigger",
      SidePanelOpenTrigger::kNewTabPageAutomaticCustomizeChrome, 0);
}

IN_PROC_BROWSER_TEST_F(NewTabPageHandlerWithCustomizeChromePromoBrowserTest,
                       DontOpenPanelWhenCustomizeButtonWasClickedBefore) {
  profile()->GetPrefs()->SetInteger(prefs::kNtpCustomizeChromeButtonOpenCount,
                                    1);

  OpenNewTabPageInForegroundAndWaitForLoad();
  EXPECT_FALSE(IsCustomizeChromeEntryShowing());

  histogram_tester_.ExpectUniqueSample(
      "NewTabPage.CustomizeChromePromoEligibility",
      NTPCustomizeChromePromoEligibility::kCustomizeChromeOpenedByUser, 1);
  histogram_tester_.ExpectUniqueSample(
      "SidePanel.OpenTrigger",
      SidePanelOpenTrigger::kNewTabPageAutomaticCustomizeChrome, 0);
}

IN_PROC_BROWSER_TEST_F(NewTabPageHandlerWithCustomizeChromePromoBrowserTest,
                       DontOpenPanelWhenPanelWasShowedMaxTimesBefore) {
  for (size_t i = 0;
       i < ntp_features::kNtpCustomizeChromeAutoShownMaxCount.Get(); ++i) {
    OpenNewTabPageInForegroundAndWaitForLoad();
    EXPECT_TRUE(IsCustomizeChromeEntryShowing());
  }

  OpenNewTabPageInForegroundAndWaitForLoad();
  EXPECT_FALSE(IsCustomizeChromeEntryShowing());

  histogram_tester_.ExpectBucketCount(
      "NewTabPage.CustomizeChromePromoEligibility",
      NTPCustomizeChromePromoEligibility::kReachedTotalMaxCountAlready, 1);
  histogram_tester_.ExpectBucketCount(
      "SidePanel.OpenTrigger",
      SidePanelOpenTrigger::kNewTabPageAutomaticCustomizeChrome,
      ntp_features::kNtpCustomizeChromeAutoShownMaxCount.Get());
}

IN_PROC_BROWSER_TEST_F(NewTabPageHandlerWithCustomizeChromePromoBrowserTest,
                       DontOpenPanelAgainWhenPanelWasExplicitlyCanceledBefore) {
  OpenNewTabPageInForegroundAndWaitForLoad();
  EXPECT_TRUE(IsCustomizeChromeEntryShowing());

  // Simulate side panel closed via explicit user action. After that, no new
  // Customize Chrome should be opened on the second NTP.
  CloseSidePanel();

  OpenNewTabPageInForegroundAndWaitForLoad();

  EXPECT_FALSE(IsCustomizeChromeEntryShowing());

  histogram_tester_.ExpectBucketCount(
      "NewTabPage.CustomizeChromePromoEligibility",
      NTPCustomizeChromePromoEligibility::
          kCustomizeChromeClosedExplicitlyByUser,
      1);
  histogram_tester_.ExpectUniqueSample(
      "SidePanel.OpenTrigger",
      SidePanelOpenTrigger::kNewTabPageAutomaticCustomizeChrome, 1);
  EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                prefs::kNtpCustomizeChromeButtonOpenCount),
            0);
}

class NewTabPageHandlerWithCustomizeChromePromoFirstNTPOnlyBrowserTest
    : public NewTabPageHandlerWithCustomizeChromePromoBrowserTest {
 protected:
  NewTabPageHandlerWithCustomizeChromePromoFirstNTPOnlyBrowserTest() {
    scoped_feature_list_first_ntp_only_.InitAndEnableFeatureWithParameters(
        ntp_features::kNtpCustomizeChromeAutoOpen,
        {{"max_customize_chrome_auto_shown_count", "5"},
         {"max_customize_chrome_auto_shown_session_count", "1"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_first_ntp_only_;
};

IN_PROC_BROWSER_TEST_F(
    NewTabPageHandlerWithCustomizeChromePromoFirstNTPOnlyBrowserTest,
    PRE_DontOpenPanelWhenPromoAutoopenedInTheSameSession) {
  OpenNewTabPageInForegroundAndWaitForLoad();
  EXPECT_TRUE(IsCustomizeChromeEntryShowing());

  OpenNewTabPageInForegroundAndWaitForLoad();
  EXPECT_FALSE(IsCustomizeChromeEntryShowing());

  histogram_tester_.ExpectUniqueSample(
      "SidePanel.OpenTrigger",
      SidePanelOpenTrigger::kNewTabPageAutomaticCustomizeChrome, 1);
}

IN_PROC_BROWSER_TEST_F(
    NewTabPageHandlerWithCustomizeChromePromoFirstNTPOnlyBrowserTest,
    DontOpenPanelWhenPromoAutoopenedInTheSameSession) {
  OpenNewTabPageInForegroundAndWaitForLoad();
  EXPECT_TRUE(IsCustomizeChromeEntryShowing());

  OpenNewTabPageInForegroundAndWaitForLoad();
  EXPECT_FALSE(IsCustomizeChromeEntryShowing());

  histogram_tester_.ExpectBucketCount(
      "NewTabPage.CustomizeChromePromoEligibility",
      NTPCustomizeChromePromoEligibility::kReachedSessionMaxCountAlready, 1);
  histogram_tester_.ExpectUniqueSample(
      "SidePanel.OpenTrigger",
      SidePanelOpenTrigger::kNewTabPageAutomaticCustomizeChrome, 1);
}

class NewTabPageHandlerWithCustomizeChromeTutorialBrowserTest
    : public InteractiveFeaturePromoTestMixin<
          NewTabPageHandlerWithCustomizeChromePromoBaseBrowserTest> {
 protected:
  NewTabPageHandlerWithCustomizeChromeTutorialBrowserTest()
      : InteractiveFeaturePromoTestMixin(UseDefaultTrackerAllowingPromos(
            {feature_engagement::
                 kIPHDesktopCustomizeChromeExperimentFeature})) {
    scoped_feature_list_iph_only_.InitAndEnableFeatureWithParameters(
        ntp_features::kNtpCustomizeChromeAutoOpen,
        // These params enables the tutorial variation.
        {{"max_customize_chrome_auto_shown_count", "0"},
         {"max_customize_chrome_auto_shown_session_count", "0"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_iph_only_;
};

IN_PROC_BROWSER_TEST_F(NewTabPageHandlerWithCustomizeChromeTutorialBrowserTest,
                       DontOpenPanelWhenTutorialShouldBeShown) {
  OpenNewTabPageInForeground();

  RunTestSequence(
      InAnyContext(WaitForShow(
          CustomizeButtonsHandler::kCustomizeChromeButtonElementId)),
      CheckPromoRequested(
          feature_engagement::kIPHDesktopCustomizeChromeExperimentFeature,
          true));
  EXPECT_FALSE(IsCustomizeChromeEntryShowing());

  histogram_tester_.ExpectBucketCount(
      "NewTabPage.CustomizeChromePromoEligibility",
      NTPCustomizeChromePromoEligibility::kCanShowPromo, 1);
  histogram_tester_.ExpectUniqueSample(
      "SidePanel.OpenTrigger",
      SidePanelOpenTrigger::kNewTabPageAutomaticCustomizeChrome, 0);
}

class NewTabPageHandlerWithCustomizeChromeIPHAutoOpenTest
    : public InteractiveFeaturePromoTestMixin<
          NewTabPageHandlerWithCustomizeChromePromoBaseBrowserTest> {
 public:
  NewTabPageHandlerWithCustomizeChromeIPHAutoOpenTest()
      : InteractiveFeaturePromoTestMixin(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHDesktopCustomizeChromeAutoOpenFeature})) {}
};

IN_PROC_BROWSER_TEST_F(
    NewTabPageHandlerWithCustomizeChromeIPHAutoOpenTest,
    PRE_ShouldShowSidePanelForTheSecondTimeIndependentlyOfIPH) {
  OpenNewTabPageInForeground();
  // TODO(crbug.com/454919411): Explicitly check whether an IPH is shown.
  RunTestSequence(
      InAnyContext(WaitForShow(
          CustomizeButtonsHandler::kCustomizeChromeButtonElementId)),
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

IN_PROC_BROWSER_TEST_F(NewTabPageHandlerWithCustomizeChromeIPHAutoOpenTest,
                       ShouldShowSidePanelForTheSecondTimeIndependentlyOfIPH) {
  OpenNewTabPageInForeground();
  RunTestSequence(
      InAnyContext(WaitForShow(
          CustomizeButtonsHandler::kCustomizeChromeButtonElementId)),
      // Promo was requested, but not necessarily shown. When fixing
      // crbug.com/454919411, that could be properly checked.
      CheckPromoRequested(
          feature_engagement::kIPHDesktopCustomizeChromeAutoOpenFeature, true),
      WaitForShow(kSidePanelElementId));

  EXPECT_TRUE(IsCustomizeChromeEntryShowing());

  EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                prefs::kNtpCustomizeChromeSidePanelAutoOpeningsCount),
            2);

  histogram_tester_.ExpectUniqueSample(
      "NewTabPage.CustomizeChromePromoEligibility",
      NTPCustomizeChromePromoEligibility::kCanShowPromo, 1);
  histogram_tester_.ExpectUniqueSample(
      "SidePanel.OpenTrigger",
      SidePanelOpenTrigger::kNewTabPageAutomaticCustomizeChrome, 1);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
class NewTabPageHandlerManagedTest : public NewTabPageHandlerBaseBrowserTest,
                                     public testing::WithParamInterface<bool> {
 public:
  NewTabPageHandlerManagedTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{ntp_features::kNtpFooter,
                              features::kEnterpriseBadgingForNtpFooter},
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
        browser(), GURL(chrome::kChromeUINewTabPageURL),
        WindowOpenDisposition::CURRENT_TAB,
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
