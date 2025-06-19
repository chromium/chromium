// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/extensions/settings_api_bubble_helpers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/new_tab_footer/footer_web_view.h"
#include "chrome/browser/ui/webui/new_tab_footer/footer_context_menu.h"
#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer_handler.h"
#include "chrome/browser/ui/webui/test_support/webui_interactive_test_mixin.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/search/ntp_features.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/test_extension_dir.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTabElementId);

using DeepQuery = WebContentsInteractionTestUtil::DeepQuery;
const DeepQuery kCustomizeChromeButton{
    "new-tab-footer-app", "ntp-customize-buttons", "#customizeButton"};

}  // namespace

class FooterInteractiveTest
    : public WebUiInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{ntp_features::kNtpFooter},
        /*disabled_features=*/{features::kSideBySide});
    InteractiveBrowserTest::SetUp();
  }

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
                 WaitForShow(kCustomizeChromeSidePanelWebViewElementId),
                 InstrumentNonTabWebView(
                     contents_id, kCustomizeChromeSidePanelWebViewElementId));
  }

  InteractiveTestApi::MultiStep OpenSidePanel(
      const ui::ElementIdentifier& contents_id) {
    return Steps(
        EnsureNotPresent(kSidePanelElementId),
        ExecuteJsAt(contents_id, kCustomizeChromeButton, "el => el.click()"),
        WaitForShow(kSidePanelElementId));
  }

  InteractiveTestApi::MultiStep CloseSidePanel(
      const ui::ElementIdentifier& contents_id) {
    return Steps(
        EnsurePresent(kSidePanelElementId),
        ExecuteJsAt(contents_id, kCustomizeChromeButton, "el => el.click()"),
        WaitForHide(kSidePanelElementId));
  }

  new_tab_footer::NewTabFooterWebView* GetFooterView() {
    return browser()->GetBrowserView().new_tab_footer_web_view();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  extensions::ScopedInstallVerifierBypassForTest install_verifier_bypass_;
};

IN_PROC_BROWSER_TEST_F(FooterInteractiveTest, FooterShowsOnExtensionNtp) {
  LoadNtpOverridingExtension();
  RunTestSequence(
      // Open extension NTP.
      AddInstrumentedTab(kNewTabElementId, GURL(chrome::kChromeUINewTabURL)),
      // Ensure footer and footer separator are visible.
      Steps(WaitForShow(kNtpFooterId),
            EnsurePresent(kFooterWebViewSeparatorElementId)));
}

IN_PROC_BROWSER_TEST_F(FooterInteractiveTest, FooterHiddenOnNonExtensionNtp) {
  LoadNtpOverridingExtension();
  RunTestSequence(
      // Open extension NTP.
      AddInstrumentedTab(kNewTabElementId, GURL(chrome::kChromeUINewTabURL)),
      // Ensure footer shows.
      WaitForShow(kNtpFooterId),
      // Navigate to non-extension NTP.
      NavigateWebContents(kNewTabElementId, GURL("https://google.com")),
      // Ensure footer hides.
      WaitForHide(kNtpFooterId));
}

IN_PROC_BROWSER_TEST_F(FooterInteractiveTest,
                       ExtensionAttributionTogglesVisibility) {
  LoadNtpOverridingExtension();
  RunTestSequence(
      // Open extension NTP.
      AddInstrumentedTab(kNewTabElementId, GURL(chrome::kChromeUINewTabURL)),
      // Ensure footer shows.
      WaitForShow(kNtpFooterId),
      // Disable extension attribution policy.
      Do([=, this]() {
        browser()->profile()->GetPrefs()->SetBoolean(
            prefs::kNTPFooterExtensionAttributionEnabled, false);
      }),
      // Ensure footer hides.
      WaitForHide(kNtpFooterId));
}

IN_PROC_BROWSER_TEST_F(FooterInteractiveTest, OpenAndCloseCustomizeChrome) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabElementId1);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabElementId2);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFooterElementId1);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFooterElementId2);

  LoadNtpOverridingExtension();
  RunTestSequence(
      // Open the first tab.
      Steps(
          AddInstrumentedTab(kTabElementId1, GURL(chrome::kChromeUINewTabURL)),
          InstrumentNonTabWebView(kFooterElementId1, kNtpFooterId)),
      // Open the side panel in the first tab.
      OpenSidePanel(kFooterElementId1),
      // Open the second tab.
      Steps(
          AddInstrumentedTab(kTabElementId2, GURL(chrome::kChromeUINewTabURL)),
          InstrumentNonTabWebView(kFooterElementId2, kNtpFooterId)),
      // Open the side panel in the second tab.
      OpenSidePanel(kFooterElementId2),
      // Close the side panel in the second tab.
      CloseSidePanel(kFooterElementId2),
      // Switch to the first tab.
      SelectTab(kTabStripElementId, 1),
      // Close the side panel in the first tab.
      CloseSidePanel(kFooterElementId1));
}

// Test is flaky on Mac, possibly due to the Mac handling of context menus.
#if !BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(FooterInteractiveTest, ContextMenuHidesFooter) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kLocalFooterElementId);

  const DeepQuery kFooterContainer = {"new-tab-footer-app", "#container"};

  // Disable the "NTP overridden" dialog as it can interfere with this
  // test.
  extensions::SetNtpPostInstallUiEnabledForTesting(false);
  // Override the ntp with an extension.
  LoadNtpOverridingExtension();
  RunTestSequence(
      // Open extension ntp.
      AddInstrumentedTab(kNewTabElementId, GURL(chrome::kChromeUINewTabURL)),
      // Right click on footer to open context menu.
      Steps(InstrumentNonTabWebView(kLocalFooterElementId, kNtpFooterId),
            MoveMouseTo(kLocalFooterElementId, kFooterContainer),
            ClickMouse(ui_controls::RIGHT)),
      // Select the "hide footer" option.
      Steps(WaitForShow(FooterContextMenu::kHideFooterIdForTesting),
            SelectMenuItem(FooterContextMenu::kHideFooterIdForTesting,
                           InputType::kMouse)),
      // Ensure footer hides.
      WaitForHide(kLocalFooterElementId));
}
#endif  // !BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Tests in this class will have a managed browser, unless the test disables it
// manually.
class FooterEnterpriseInteractiveTest : public FooterInteractiveTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{ntp_features::kNtpFooter,
                              features::kEnterpriseBadgingForNtpFooter},
        /*disabled_features=*/{features::kSideBySide});
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    scoped_browser_management_ =
        std::make_unique<policy::ScopedManagementServiceOverrideForTesting>(
            policy::ManagementServiceFactory::GetForProfile(
                browser()->profile()),
            policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    FooterInteractiveTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    scoped_browser_management_.reset();
    FooterInteractiveTest::TearDownOnMainThread();
  }

 private:
  std::unique_ptr<policy::ScopedManagementServiceOverrideForTesting>
      scoped_browser_management_;
};

IN_PROC_BROWSER_TEST_F(FooterEnterpriseInteractiveTest, FooterShowsOnNtpOnly) {
  LoadNtpOverridingExtension();
  RunTestSequence(
      // Open extension NTP.
      AddInstrumentedTab(kNewTabElementId, GURL(chrome::kChromeUINewTabURL)),
      // Ensure footer shows.
      WaitForShow(kNtpFooterId),
      // Navigate to non-NTP.
      NavigateWebContents(kNewTabElementId, GURL("https://google.com")),
      // Ensure footer hides.
      WaitForHide(kNtpFooterId),
      // Navigate to 1P WebUI NTP.
      NavigateWebContents(kNewTabElementId,
                          GURL(chrome::kChromeUINewTabPageURL)),
      // Ensure footer shows.
      WaitForShow(kNtpFooterId));
}

IN_PROC_BROWSER_TEST_F(FooterEnterpriseInteractiveTest,
                       ManagementNoticePolicyTogglesVisibility) {
  RunTestSequence(
      // Open NTP.
      AddInstrumentedTab(kNewTabElementId, GURL(chrome::kChromeUINewTabURL)),
      // Ensure footer shows.
      WaitForShow(kNtpFooterId),
      // Disable management notice policy.
      Do([=]() {
        g_browser_process->local_state()->SetBoolean(
            prefs::kNTPFooterManagementNoticeEnabled, false);
      }),
      // Ensure footer hides.
      WaitForHide(kNtpFooterId));
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
