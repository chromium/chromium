// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/views/extensions/extensions_tabbed_menu_view.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_browsertest.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/test/test_extension_dir.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/test/widget_test.h"

class ExtensionsTabbedMenuViewInteractiveUITest
    : public ExtensionsToolbarBrowserTest {
 public:
  ExtensionsTabbedMenuViewInteractiveUITest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kExtensionsMenuAccessControl);
  }
  ExtensionsTabbedMenuViewInteractiveUITest(
      const ExtensionsTabbedMenuViewInteractiveUITest&) = delete;
  ExtensionsTabbedMenuViewInteractiveUITest& operator=(
      const ExtensionsTabbedMenuViewInteractiveUITest&) = delete;
  ~ExtensionsTabbedMenuViewInteractiveUITest() override = default;

  void ShowUi(const std::string& name) override { NOTREACHED(); }

  void ClickToolbarButton(ExtensionsToolbarButton* button) {
    ui::MouseEvent click_event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                               base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, 0);
    button->OnMousePressed(click_event);
    views::test::WaitForAnimatingLayoutManager(GetExtensionsToolbarContainer());
  }

  ExtensionsToolbarButton* extensions_button() {
    return GetExtensionsToolbarContainer()
        ->GetExtensionsToolbarControls()
        ->extensions_button();
  }

  ExtensionsToolbarButton* site_access_button() {
    return GetExtensionsToolbarContainer()
        ->GetExtensionsToolbarControls()
        ->site_access_button_for_testing();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/1268996): Fix flakiness on Windows and Linux and reenable.
#if defined(OS_WIN) || defined(OS_LINUX)
#define MAYBE_ExtensionsButtonOpensInstalledExtensionsTab \
  DISABLED_ExtensionsButtonOpensInstalledExtensionsTab
#else
#define MAYBE_ExtensionsButtonOpensInstalledExtensionsTab \
  ExtensionsButtonOpensInstalledExtensionsTab
#endif
IN_PROC_BROWSER_TEST_F(ExtensionsTabbedMenuViewInteractiveUITest,
                       MAYBE_ExtensionsButtonOpensInstalledExtensionsTab) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Load extension with all urls permissions so the site access button can be
  // visible.
  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
           "name": "All Urls Extension",
           "manifest_version": 3,
           "version": "0.1",
           "host_permissions": ["<all_urls>"]
         })");
  AppendExtension(
      extensions::ChromeTestExtensionLoader(profile()).LoadExtension(
          test_dir.UnpackedPath()));
  ASSERT_EQ(1u, extensions().size());
  views::test::WaitForAnimatingLayoutManager(GetExtensionsToolbarContainer());

  // Click on the extensions button when the menu is closed. Extensions menu
  // should open in the installed extensions tab.
  ClickToolbarButton(extensions_button());
  EXPECT_EQ(ExtensionsTabbedMenuView::GetExtensionsTabbedMenuViewForTesting()
                ->GetSelectedTabIndex(),
            1u);

  // Click on the extensions button when the menu is open. Extensions menu
  // should be closed.
  views::test::WidgetDestroyedWaiter destroyed_waiter(
      ExtensionsTabbedMenuView::GetExtensionsTabbedMenuViewForTesting()
          ->GetWidget());
  ClickToolbarButton(extensions_button());
  destroyed_waiter.Wait();
  EXPECT_FALSE(ExtensionsTabbedMenuView::IsShowing());

  // Navigate to a URL for the site access button to appear.
  GURL url = embedded_test_server()->GetURL("a.com", "/title1.html");
  content::TestNavigationObserver observer(web_contents);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  observer.WaitForNavigationFinished();
  views::test::WaitForAnimatingLayoutManager(GetExtensionsToolbarContainer());
  ASSERT_TRUE(site_access_button());

  // Click on the site access button when the menu is closed. Extensions menu
  // should open in the site access tab.
  ClickToolbarButton(site_access_button());
  EXPECT_EQ(ExtensionsTabbedMenuView::GetExtensionsTabbedMenuViewForTesting()
                ->GetSelectedTabIndex(),
            0u);
}
