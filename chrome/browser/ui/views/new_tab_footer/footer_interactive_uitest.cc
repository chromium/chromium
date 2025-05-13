// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/new_tab_footer/footer_web_view.h"
#include "chrome/browser/ui/webui/test_support/webui_interactive_test_mixin.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/search/ntp_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/test/test_extension_dir.h"

namespace {
constexpr char kFooterViewName[] = "footer_view";
}  // namespace

class FooterInteractiveTest
    : public WebUiInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  FooterInteractiveTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{ntp_features::kNtpFooter},
        /*disabled_features=*/{features::kSideBySide});
  }

  void LoadNtpOverridingExtension(Profile* profile) {
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
    const extensions::Extension* extension =
        extension_loader.LoadExtension(extension_dir.Pack()).get();
    ASSERT_TRUE(extension);
  }

  void OpenNewTabPage() {
    chrome::NewTab(browser());

    // Wait until navigation to chrome://newtab finishes.
    content::TestNavigationObserver nav_observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    nav_observer.Wait();
  }

  void NavigateTo(const GURL& url) {
    // Wait until navigation to `url` finishes.
    content::TestNavigationObserver nav_observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    nav_observer.Wait();
  }

  new_tab_footer::NewTabFooterWebView* GetFooterView() {
    return browser()->GetBrowserView().new_tab_footer_web_view();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  extensions::ScopedInstallVerifierBypassForTest install_verifier_bypass_;
};

IN_PROC_BROWSER_TEST_F(FooterInteractiveTest, FooterVisibleOnExtensionNtp) {
  LoadNtpOverridingExtension(browser()->profile());
  RunTestSequence(
      // Open extension NTP.
      Do(base::BindLambdaForTesting([&, this]() { OpenNewTabPage(); })),
      Steps(NameView(kFooterViewName, GetFooterView()),
            // Ensure footer is visible.
            CheckView(kFooterViewName,
                      [](new_tab_footer::NewTabFooterWebView* footer) {
                        return footer->GetVisible();
                      })));
}

IN_PROC_BROWSER_TEST_F(FooterInteractiveTest,
                       FooterNotVisibleOnNonExtensionNtp) {
  LoadNtpOverridingExtension(browser()->profile());
  RunTestSequence(
      // Open extension NTP.
      Do(base::BindLambdaForTesting([&, this]() { OpenNewTabPage(); })),
      Steps(NameView(kFooterViewName, GetFooterView()),
            // Ensure footer is visible.
            CheckView(kFooterViewName,
                      [](new_tab_footer::NewTabFooterWebView* footer) {
                        return footer->GetVisible();
                      })),
      // Navigate to non-extension NTP and check that the footer isn't visible.
      Do(base::BindLambdaForTesting(
          [&, this]() { NavigateTo(GURL("https://google.com")); })),
      WaitForHide(kFooterViewName));
}
