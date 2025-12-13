// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/extensions/settings_api_bubble_helpers.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/contents_container_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/browser/ui/views/new_tab_footer/footer_web_view.h"
#include "chrome/browser/ui/webui/new_tab_footer/footer_context_menu.h"
#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer_handler.h"
#include "chrome/browser/ui/webui/test_support/webui_interactive_test_mixin.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/install_verifier.h"
#include "extensions/test/test_extension_dir.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/view_class_properties.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTabElementId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFooterLocalElementId);

using DeepQuery = WebContentsInteractionTestUtil::DeepQuery;
const DeepQuery kFooterCustomizeChromeButton{
    "new-tab-footer-app", "ntp-customize-buttons", "#customizeButton"};

}  // namespace

class FooterInteractiveTestBase
    : public WebUiInteractiveTestMixin<InteractiveBrowserTest>,
      public testing::WithParamInterface<bool> {
 public:
  void SetUpOnMainThread() override {
    WebUiInteractiveTestMixin::SetUpOnMainThread();
    browser()->GetProfile()->GetPrefs()->SetBoolean(prefs::kNtpFooterVisible,
                                                    true);
  }

  void LoadNtpOverridingExtension() {
    extensions::TestExtensionDir extension_dir;
    extension_dir.WriteFile(FILE_PATH_LITERAL("ext.html"),
                            "<body>Extension-overridden NTP</body>");

    const char extension_manifest[] = R"(
       {
           "chrome_url_overrides": {
               "newtab": "ext.html"
           },
           "name": "Extension-overridden NTP",
           "manifest_version": 3,
           "version": "0.1"
         })";

    extension_dir.WriteManifest(extension_manifest);

    extensions::ChromeTestExtensionLoader extension_loader(
        browser()->profile());
    extension_loader.set_ignore_manifest_warnings(true);
    const extensions::Extension* extension =
        extension_loader.LoadExtension(extension_dir.Pack()).get();
    ASSERT_TRUE(extension);
  }

  InteractiveTestApi::MultiStep OpenCustomizeChromeSidePanel(
      const ui::ElementIdentifier& contents_id) {
    return Steps(Do(base::BindLambdaForTesting([=, this]() {
                   chrome::ExecuteCommand(browser(),
                                          IDC_SHOW_CUSTOMIZE_CHROME_SIDE_PANEL);
                 })),
                 InstrumentNonTabWebView(
                     contents_id, kCustomizeChromeSidePanelWebViewElementId));
  }

  InteractiveTestApi::MultiStep OpenSidePanel(
      const ui::ElementIdentifier& contents_id) {
    return Steps(EnsureNotPresent(kSidePanelElementId),
                 ExecuteJsAt(contents_id, kFooterCustomizeChromeButton,
                             "el => el.click()"),
                 WaitForShow(kSidePanelElementId));
  }

  InteractiveTestApi::MultiStep CloseSidePanel(
      const ui::ElementIdentifier& contents_id) {
    return Steps(EnsurePresent(kSidePanelElementId),
                 ExecuteJsAt(contents_id, kFooterCustomizeChromeButton,
                             "el => el.click()"),
                 WaitForHide(kSidePanelElementId));
  }

  InteractiveTestApi::MultiStep OpenContextMenuAndSelect(
      const ui::ElementIdentifier& menu_item_id) {
    // Disable the "NTP overridden" dialog as it can interfere with this
    // test.
    extensions::SetNtpPostInstallUiEnabledForTesting(false);
    const DeepQuery kFooterContainer = {"new-tab-footer-app", "#container"};
    return Steps(
        InstrumentNonTabWebView(kFooterLocalElementId, kNtpFooterViewElementId),
        MoveMouseTo(kFooterLocalElementId, kFooterContainer),
        ClickMouse(ui_controls::RIGHT), WaitForShow(menu_item_id),
        SelectMenuItem(menu_item_id, InputType::kMouse));
  }

  InteractiveTestApi::MultiStep WaitForElementExists(
      const ui::ElementIdentifier& contents_id,
      const DeepQuery& element) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementExists);
    StateChange element_exists;
    element_exists.type = StateChange::Type::kExists;
    element_exists.where = element;
    element_exists.event = kElementExists;
    return WaitForStateChange(contents_id, element_exists);
  }

  new_tab_footer::NewTabFooterWebView* GetFooterView() {
    return browser()
        ->GetBrowserView()
        .GetActiveContentsContainerView()
        ->new_tab_footer_view();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  extensions::ScopedInstallVerifierBypassForTest install_verifier_bypass_;
};

class FooterInteractiveTest : public FooterInteractiveTestBase {
 public:
  FooterInteractiveTest() {
    scoped_feature_list_.InitWithFeatureStates(
        {{ntp_features::kNtpFooter, true},
         {features::kSideBySide, GetParam()}});
  }
  ~FooterInteractiveTest() override = default;
};

INSTANTIATE_TEST_SUITE_P(, FooterInteractiveTest, testing::Bool());

IN_PROC_BROWSER_TEST_P(FooterInteractiveTest, FooterShowsOnExtensionNtp) {
  LoadNtpOverridingExtension();
  RunTestSequence(
      // Open extension NTP.
      AddInstrumentedTab(kNewTabElementId, GURL(chrome::kChromeUINewTabURL)),
      // Ensure footer and footer separator are visible.
      Steps(WaitForShow(kNtpFooterViewElementId),
            EnsurePresent(kFooterWebViewSeparatorElementId)));
}

IN_PROC_BROWSER_TEST_P(FooterInteractiveTest, FooterHiddenOnNonExtensionNtp) {
  LoadNtpOverridingExtension();
  RunTestSequence(
      // Open extension NTP.
      AddInstrumentedTab(kNewTabElementId, GURL(chrome::kChromeUINewTabURL)),
      // Ensure footer shows.
      WaitForShow(kNtpFooterViewElementId),
      // Navigate to non-extension NTP.
      NavigateWebContents(kNewTabElementId, GURL("https://google.com")),
      // Ensure footer hides.
      WaitForHide(kNtpFooterViewElementId));
}

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_P(FooterInteractiveTest, FooterHidesInGuestProfile) {
  LoadNtpOverridingExtension();
  Browser* const guest_browser = CreateGuestBrowser();
  ui_test_utils::BrowserActivationWaiter(guest_browser).WaitForActivation();

  RunTestSequenceInContext(
      // Run the following steps with the guest browser as the default context.
      BrowserElements::From(guest_browser)->GetContext(),
      // Open NTP in guest profile.
      AddInstrumentedTab(kNewTabElementId, GURL(chrome::kChromeUINewTabURL)),
      // Ensure footer is not present.
      EnsureNotPresent(kNtpFooterViewElementId));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_P(FooterInteractiveTest, FooterHidesInIncognito) {
  LoadNtpOverridingExtension();
  Browser* const incognito_browser = CreateIncognitoBrowser();
  ui_test_utils::BrowserActivationWaiter(incognito_browser).WaitForActivation();

  RunTestSequenceInContext(
      // Run the following steps with the incognito browser as the default
      // context.
      BrowserElements::From(incognito_browser)->GetContext(),
      // Open NTP in incognito window.
      AddInstrumentedTab(kNewTabElementId, GURL(chrome::kChromeUINewTabURL)),
      // Ensure footer is not present.
      EnsureNotPresent(kNtpFooterViewElementId));
}

IN_PROC_BROWSER_TEST_P(FooterInteractiveTest,
                       ExtensionAttributionTogglesVisibility) {
  LoadNtpOverridingExtension();
  RunTestSequence(
      // Open extension NTP.
      AddInstrumentedTab(kNewTabElementId, GURL(chrome::kChromeUINewTabURL)),
      // Ensure footer shows.
      WaitForShow(kNtpFooterViewElementId),
      // Disable extension attribution policy.
      Do([=, this]() {
        browser()->profile()->GetPrefs()->SetBoolean(
            prefs::kNTPFooterExtensionAttributionEnabled, false);
      }),
      // Ensure footer hides.
      WaitForHide(kNtpFooterViewElementId));
}

IN_PROC_BROWSER_TEST_P(FooterInteractiveTest, OpenAndCloseCustomizeChrome) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabElementId1);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabElementId2);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFooterElementId1);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFooterElementId2);

  LoadNtpOverridingExtension();
  RunTestSequence(
      // Open the first tab.
      Steps(
          AddInstrumentedTab(kTabElementId1, GURL(chrome::kChromeUINewTabURL)),
          InstrumentNonTabWebView(kFooterElementId1, kNtpFooterViewElementId)),
      // Open the side panel in the first tab.
      OpenSidePanel(kFooterElementId1),
      // Open the second tab.
      Steps(
          AddInstrumentedTab(kTabElementId2, GURL(chrome::kChromeUINewTabURL)),
          InstrumentNonTabWebView(kFooterElementId2, kNtpFooterViewElementId)),
      // Open the side panel in the second tab.
      OpenSidePanel(kFooterElementId2),
      // Close the side panel in the second tab.
      CloseSidePanel(kFooterElementId2),
      // Switch to the first tab.
      SelectTab(kTabStripElementId, 1),
      // Close the side panel in the first tab.
      CloseSidePanel(kFooterElementId1));
}

// Context menu tests flaky on Mac, possibly due to the Mac handling of context
// menus.
#if !BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_P(FooterInteractiveTest, ContextMenuHidesFooter) {
  // Override the ntp with an extension.
  LoadNtpOverridingExtension();
  RunTestSequence(
      // Open extension ntp.
      AddInstrumentedTab(kNewTabElementId, GURL(chrome::kChromeUINewTabURL)),
      // Open context menu and select "hide footer" option.
      OpenContextMenuAndSelect(FooterContextMenu::kHideFooterIdForTesting),
      // Ensure footer hides.
      WaitForHide(kFooterLocalElementId));
}

IN_PROC_BROWSER_TEST_P(FooterInteractiveTest, ContextMenuOpensCustomizeChrome) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kLocalCustomizeChromeElementId);
  const DeepQuery kFooterSection = {"customize-chrome-app", "#footer",
                                    "customize-chrome-footer",
                                    "#showToggleContainer"};

  // Override the ntp with an extension.
  LoadNtpOverridingExtension();
  RunTestSequence(
      // Open extension ntp.
      AddInstrumentedTab(kNewTabElementId, GURL(chrome::kChromeUINewTabURL)),
      // Open context menu and select "customize chrome" option.
      OpenContextMenuAndSelect(
          FooterContextMenu::kShowCustomizeChromeIdForTesting),
      // Ensure customize chrome opens to footer section.
      Steps(
          InstrumentNonTabWebView(kLocalCustomizeChromeElementId,
                                  kCustomizeChromeSidePanelWebViewElementId,
                                  false),
          WaitForElementExists(kLocalCustomizeChromeElementId, kFooterSection),
          WaitForElementToRender(kLocalCustomizeChromeElementId,
                                 kFooterSection)));
}
#endif  // !BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Tests in this class will have a managed browser, unless the test disables it
// manually.
class FooterEnterpriseInteractiveTest : public FooterInteractiveTestBase {
 public:
  FooterEnterpriseInteractiveTest() {
    scoped_feature_list_.InitWithFeatureStates(
        {{ntp_features::kNtpFooter, true},
         {features::kEnterpriseBadgingForNtpFooter, true},
         {features::kSideBySide, GetParam()}});
  }
  ~FooterEnterpriseInteractiveTest() override = default;

  void SetUpOnMainThread() override {
    scoped_browser_management_ =
        std::make_unique<policy::ScopedManagementServiceOverrideForTesting>(
            policy::ManagementServiceFactory::GetForProfile(
                browser()->profile()),
            policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    FooterInteractiveTestBase::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    scoped_browser_management_.reset();
    incognito_scoped_browser_management_.reset();
    guest_scoped_browser_management_.reset();
    FooterInteractiveTestBase::TearDownOnMainThread();
  }

  InteractiveTestApi::MultiStep OpenNewTabAndWaitForFooter(const GURL& url) {
    return Steps(
        // Open a new tab for url.
        AddInstrumentedTab(kNewTabElementId, url),
        // Wait for footer to show.
        InstrumentNonTabWebView(kFooterLocalElementId,
                                kNtpFooterViewElementId));
  }

  void SetCustomBackground() {
    auto* ntp_custom_background_service =
        NtpCustomBackgroundServiceFactory::GetForProfile(browser()->profile());
    ntp_custom_background_service->AddValidBackdropUrlForTesting(
        GURL("https://background.com"));
    ntp_custom_background_service->SetCustomBackgroundInfo(
        /*background_url=*/GURL("https://background.com"),
        /*thumbnail_url=*/GURL("https://thumbnail.com"),
        /*attribution_line_1=*/"line 1",
        /*attribution_line_2=*/"line 2",
        /*action_url=*/GURL("https://action.com"),
        /*collection_id=*/"");
  }

  Browser* CreateManagedGuestBrowser() {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    base::FilePath guest_path = profile_manager->GetGuestProfilePath();
    Profile& guest_profile =
        profiles::testing::CreateProfileSync(profile_manager, guest_path);
    Profile* guest_profile_otr =
        guest_profile.GetPrimaryOTRProfile(/*create_if_needed=*/true);
    guest_scoped_browser_management_ =
        std::make_unique<policy::ScopedManagementServiceOverrideForTesting>(
            policy::ManagementServiceFactory::GetForProfile(guest_profile_otr),
            policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);

    // Create browser and add tab.
    Browser* guest_browser =
        Browser::Create(Browser::CreateParams(guest_profile_otr, true));
    AddBlankTabAndShow(guest_browser);
    ui_test_utils::BrowserActivationWaiter(guest_browser).WaitForActivation();
    return guest_browser;
  }

  Browser* CreateManagedIncognitoBrowser() {
    Browser* incognito_browser = Browser::Create(Browser::CreateParams(
        browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
        true));
    incognito_scoped_browser_management_ =
        std::make_unique<policy::ScopedManagementServiceOverrideForTesting>(
            policy::ManagementServiceFactory::GetForProfile(
                incognito_browser->profile()),
            policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    AddBlankTabAndShow(incognito_browser);
    ui_test_utils::BrowserActivationWaiter(incognito_browser)
        .WaitForActivation();
    return incognito_browser;
  }

 private:
  std::unique_ptr<policy::ScopedManagementServiceOverrideForTesting>
      scoped_browser_management_;
  std::unique_ptr<policy::ScopedManagementServiceOverrideForTesting>
      incognito_scoped_browser_management_;
  std::unique_ptr<policy::ScopedManagementServiceOverrideForTesting>
      guest_scoped_browser_management_;
};

INSTANTIATE_TEST_SUITE_P(, FooterEnterpriseInteractiveTest, testing::Bool());

IN_PROC_BROWSER_TEST_P(FooterEnterpriseInteractiveTest, FooterShowsOnNtpOnly) {
  LoadNtpOverridingExtension();
  RunTestSequence(
      // Open extension NTP.
      AddInstrumentedTab(kNewTabElementId, GURL(chrome::kChromeUINewTabURL)),
      // Ensure footer shows.
      WaitForShow(kNtpFooterViewElementId),
      // Navigate to non-NTP.
      NavigateWebContents(kNewTabElementId, GURL("https://google.com")),
      // Ensure footer hides.
      WaitForHide(kNtpFooterViewElementId),
      // Navigate to 1P WebUI NTP.
      NavigateWebContents(kNewTabElementId,
                          GURL(chrome::kChromeUINewTabPageURL)),
      // Ensure footer shows.
      WaitForShow(kNtpFooterViewElementId));
}

IN_PROC_BROWSER_TEST_P(FooterEnterpriseInteractiveTest,
                       ManagementNoticePolicyTogglesVisibility) {
  RunTestSequence(
      // Open NTP.
      AddInstrumentedTab(kNewTabElementId, GURL(chrome::kChromeUINewTabURL)),
      // Ensure footer shows.
      WaitForShow(kNtpFooterViewElementId),
      // Disable management notice policy.
      Do([=]() {
        g_browser_process->local_state()->SetBoolean(
            prefs::kNTPFooterManagementNoticeEnabled, false);
      }),
      // Ensure footer hides.
      WaitForHide(kNtpFooterViewElementId));
}

IN_PROC_BROWSER_TEST_P(FooterEnterpriseInteractiveTest,
                       CustomizationTogglesVisibility) {
  RunTestSequence(
      // Open NTP.
      AddInstrumentedTab(kNewTabElementId, GURL(chrome::kChromeUINewTabURL)),
      // Ensure footer shows.
      WaitForShow(kNtpFooterViewElementId),
      // Toggle off visibility.
      Do([=, this]() {
        browser()->GetProfile()->GetPrefs()->SetBoolean(
            prefs::kNtpFooterVisible, false);
      }),
      // Ensure footer hides.
      WaitForHide(kNtpFooterViewElementId),
      // Set a custom label policy.
      Do([=]() {
        g_browser_process->local_state()->SetString(
            prefs::kEnterpriseCustomLabelForBrowser, "Custom Label");
      }),
      // Ensure footer shows
      WaitForShow(kNtpFooterViewElementId),
      // Unset the custom label policy.
      Do([=]() {
        g_browser_process->local_state()->SetString(
            prefs::kEnterpriseCustomLabelForBrowser, "");
      }),
      // Ensure footer hides.
      WaitForHide(kNtpFooterViewElementId));
}

IN_PROC_BROWSER_TEST_P(FooterEnterpriseInteractiveTest,
                       FooterShowsInGuestProfile) {
  // Create browser and add tab.
  Browser* guest_browser = CreateManagedGuestBrowser();
  RunTestSequenceInContext(
      // Run the following steps with the guest browser as the default context.
      BrowserElements::From(guest_browser)->GetContext(),
      // Open NTP in guest profile.
      AddInstrumentedTab(kNewTabElementId, GURL(chrome::kChromeUINewTabURL)),
      // Ensure footer shows.
      WaitForShow(kNtpFooterViewElementId));
}

IN_PROC_BROWSER_TEST_P(FooterEnterpriseInteractiveTest,
                       FooterShowsInIncognito) {
  Browser* incognito_browser = CreateManagedIncognitoBrowser();
  RunTestSequenceInContext(
      // Run the following steps with the incognito browser as the default
      // context.
      BrowserElements::From(incognito_browser)->GetContext(),
      // Open NTP in incognito window.
      AddInstrumentedTab(kNewTabElementId, GURL(chrome::kChromeUINewTabURL)),
      // Ensure footer shows.
      WaitForShow(kNtpFooterViewElementId));
}

IN_PROC_BROWSER_TEST_P(FooterEnterpriseInteractiveTest,
                       CustomizeChromeButtonShowsCorrectly) {
  const DeepQuery kNtpCustomizeChromeButton = {
      "ntp-app", "ntp-customize-buttons", "#customizeButton"};
  RunTestSequence(
      // Open 1P WebUI NTP and wait for footer to show.
      OpenNewTabAndWaitForFooter(GURL(chrome::kChromeUINewTabPageURL)),
      // Ensure customize chrome button only shows in footer and not on NTP.
      Steps(EnsurePresent(kFooterLocalElementId, kFooterCustomizeChromeButton),
            EnsureNotPresent(kNewTabElementId, kNtpCustomizeChromeButton)),
      Do([=]() {
        // Disable management notice to hide footer.
        g_browser_process->local_state()->SetBoolean(
            prefs::kNTPFooterManagementNoticeEnabled, false);
      }),
      // Ensure footer hides.
      WaitForHide(kNtpFooterViewElementId),
      // Ensure customize chrome button shows in NTP.
      WaitForElementToRender(kNewTabElementId, kNtpCustomizeChromeButton));
}

IN_PROC_BROWSER_TEST_P(FooterEnterpriseInteractiveTest,
                       ThirdPartyNtpHidesCustomizeChromeButton) {
  RunTestSequence(
      // Open 3P WebUI NTP and wait for footer to show.
      OpenNewTabAndWaitForFooter(
          GURL(chrome::kChromeUINewTabPageThirdPartyURL)),
      // Ensure customize chrome button hides in footer.
      EnsureNotPresent(kFooterLocalElementId, kFooterCustomizeChromeButton));
}

IN_PROC_BROWSER_TEST_P(FooterEnterpriseInteractiveTest,
                       BackgroundAttributionShowsCorrectly) {
  const DeepQuery kNtpBackgroundAttribution = {"ntp-app",
                                               "#backgroundImageAttribution"};
  const DeepQuery kFooterBackgroundAttribution{
      "new-tab-footer-app", "#backgroundAttributionContainer"};
  RunTestSequence(
      // Open 1P WebUI NTP and wait for footer to show.
      OpenNewTabAndWaitForFooter(GURL(chrome::kChromeUINewTabPageURL)),
      // Ensure background attribution shows in footer and not on NTP.
      Steps(
          EnsureNotPresent(kFooterLocalElementId, kFooterBackgroundAttribution),
          EnsureNotPresent(kNewTabElementId, kNtpBackgroundAttribution)),
      // Set a custom background.
      Do(base::BindOnce(&FooterEnterpriseInteractiveTest::SetCustomBackground,
                        base::Unretained(this))),
      // Ensure background attribution shows in footer and not on NTP.

      Steps(EnsurePresent(kFooterLocalElementId, kFooterBackgroundAttribution),
            EnsureNotPresent(kNewTabElementId, kNtpBackgroundAttribution)),
      Do([=]() {
        // Disable management notice to hide footer.
        g_browser_process->local_state()->SetBoolean(
            prefs::kNTPFooterManagementNoticeEnabled, false);
      }),
      // Ensure footer hides.
      WaitForHide(kNtpFooterViewElementId),
      // Ensure background attribution shows in NTP.
      WaitForElementToRender(kNewTabElementId, kNtpBackgroundAttribution));
}

IN_PROC_BROWSER_TEST_P(FooterEnterpriseInteractiveTest,
                       BackgroundAttributionHidesOnThirdPartyNtp) {
  const DeepQuery kFooterBackgroundAttribution{
      "new-tab-footer-app", "#backgroundAttributionContainer"};

  RunTestSequence(
      // Set a custom background.
      Do(base::BindOnce(&FooterEnterpriseInteractiveTest::SetCustomBackground,
                        base::Unretained(this))),
      // Open 3P WebUI NTP and wait for footer to show.
      OpenNewTabAndWaitForFooter(
          GURL(chrome::kChromeUINewTabPageThirdPartyURL)),
      // Ensure background attribution hides in footer.
      EnsureNotPresent(kFooterLocalElementId, kFooterBackgroundAttribution));
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

class FooterSideBySideInteractiveTest : public FooterInteractiveTest {
 public:
  auto CheckFooterVisibility(size_t content_container_index,
                             bool is_footer_visible) {
    return CheckView(
        kMultiContentsViewElementId,
        [content_container_index,
         is_footer_visible](MultiContentsView* multi_contents_view) -> bool {
          auto contents_container_views =
              multi_contents_view->contents_container_views();
          EXPECT_LT(content_container_index, contents_container_views.size());
          return contents_container_views[content_container_index]
                     ->new_tab_footer_view()
                     ->GetVisible() == is_footer_visible;
        });
  }
};

INSTANTIATE_TEST_SUITE_P(,
                         FooterSideBySideInteractiveTest,
                         testing::Values(true));

IN_PROC_BROWSER_TEST_P(FooterSideBySideInteractiveTest, SplitNewTabPage) {
  // Disable the "NTP overridden" dialog as it can interfere with this test.
  extensions::SetNtpPostInstallUiEnabledForTesting(false);

  LoadNtpOverridingExtension();
  RunTestSequence(
      // Create a non-split tab with footer showing.
      AddInstrumentedTab(kNewTabElementId, GURL(chrome::kChromeUINewTabURL)),
      WaitForShow(kNtpFooterViewElementId),
      // Navigate to the first tab and create a new split tab, so that the tab
      // picker screen is showing on the other tab in the split.
      Do([=, this]() {
        browser()->tab_strip_model()->ExecuteContextMenuCommand(
            0, TabStripModel::ContextMenuCommand::CommandAddToSplit);
      }),
      WaitForShow(kNtpFooterViewElementId, true),
      CheckFooterVisibility(0, false), CheckFooterVisibility(1, true),
      // Navigate away from new tab page and verify footer is hidden.
      NavigateWebContents(kNewTabElementId, GURL(url::kAboutBlankURL)),
      CheckFooterVisibility(0, false), CheckFooterVisibility(1, false));
}
