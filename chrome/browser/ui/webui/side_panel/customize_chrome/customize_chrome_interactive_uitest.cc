// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/webui/test_support/webui_interactive_test_mixin.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/search/ntp_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/test/test_extension_dir.h"

namespace {
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementExists);

class CustomizeChromeInteractiveTest
    : public WebUiInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  CustomizeChromeInteractiveTest() {
    scoped_feature_list_.InitAndEnableFeature(ntp_features::kNtpFooter);
  }

  InteractiveTestApi::MultiStep WaitForElementExists(
      const ui::ElementIdentifier& contents_id,
      const DeepQuery& element) {
    StateChange element_exists;
    element_exists.type =
        WebContentsInteractionTestUtil::StateChange::Type::kExists;
    element_exists.event = kElementExists;
    element_exists.where = element;
    return WaitForStateChange(contents_id, element_exists);
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

  // Installs an extension and overrides ntp.
  void InstallExtension(Profile* profile) {
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

    extensions::ChromeTestExtensionLoader extension_loader(profile);
    extension_loader.set_ignore_manifest_warnings(true);
    // TODO(temao) Not blocking the test, but note that LoadExtension()
    // occasionally returns null.
    extension_loader.LoadExtension(extension_dir.Pack()).get();
  }

  void OpenExtensionNewTabPage() {
    chrome::NewTab(browser());
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    // Wait until chrome://newtab navigation finished.
    content::TestNavigationObserver nav_observer(web_contents);
    nav_observer.Wait();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};
}  // namespace

IN_PROC_BROWSER_TEST_F(CustomizeChromeInteractiveTest,
                       EditThemeEnabledForExtensionNtp) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kLocalCustomizeChromeElementId);

  const DeepQuery kEditThemeButton = {"customize-chrome-app",
                                      "#appearanceElement", "#editThemeButton"};

  // 1. Load extension that overrides NTP.
  InstallExtension(browser()->profile());
  RunTestSequence(
      // 2. Open extension new tab page.
      Do(base::BindLambdaForTesting(
          [&, this]() { OpenExtensionNewTabPage(); })),
      // 3. Open customize chrome side panel.
      OpenCustomizeChromeSidePanel(kLocalCustomizeChromeElementId),
      // 4. Check edit theme is enabled in customize chrome side panel.
      Steps(WaitForElementExists(kLocalCustomizeChromeElementId,
                                 kEditThemeButton),
            WaitForElementToRender(kLocalCustomizeChromeElementId,
                                   kEditThemeButton)));
}
