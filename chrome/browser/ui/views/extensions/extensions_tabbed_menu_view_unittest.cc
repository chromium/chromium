// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/views/extensions/extensions_tabbed_menu_view.h"

#include "base/feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_unittest.h"

class ExtensionsTabbedMenuViewUnitTest : public ExtensionsToolbarUnitTest {
 public:
  ExtensionsTabbedMenuViewUnitTest();
  ~ExtensionsTabbedMenuViewUnitTest() override = default;
  ExtensionsTabbedMenuViewUnitTest(const ExtensionsTabbedMenuViewUnitTest&) =
      delete;
  ExtensionsTabbedMenuViewUnitTest& operator=(
      const ExtensionsTabbedMenuViewUnitTest&) = delete;

  ExtensionsToolbarButton* extensions_button() {
    return extensions_container()
        ->GetExtensionsToolbarControls()
        ->extensions_button();
  }
  ExtensionsToolbarButton* site_access_button() {
    return extensions_container()
        ->GetExtensionsToolbarControls()
        ->site_access_button_for_testing();
  }
  ExtensionsTabbedMenuView* extensions_tabbed_menu() {
    return ExtensionsTabbedMenuView::GetExtensionsTabbedMenuViewForTesting();
  }

  void ClickSiteAccessButton();
  void ClickExtensionsButton();

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

ExtensionsTabbedMenuViewUnitTest::ExtensionsTabbedMenuViewUnitTest() {
  scoped_feature_list_.InitAndEnableFeature(
      features::kExtensionsMenuAccessControl);
}

void ExtensionsTabbedMenuViewUnitTest::ClickSiteAccessButton() {
  ClickButton(site_access_button());
  LayoutContainerIfNecessary();
}

void ExtensionsTabbedMenuViewUnitTest::ClickExtensionsButton() {
  ClickButton(extensions_button());
  LayoutContainerIfNecessary();
}

TEST_F(ExtensionsTabbedMenuViewUnitTest, ButtonOpensAndClosesCorrespondingTab) {
  content::WebContentsTester* web_contents_tester =
      AddWebContentsAndGetTester();

  // Load an extension with all urls permissions so the site access button is
  // visible.
  InstallExtensionWithHostPermissions("all_urls", {"<all_urls>"});

  // Navigate to an url where the extension should have access to.
  const GURL url("http://www.a.com");
  web_contents_tester->NavigateAndCommit(url);
  WaitForAnimation();
  EXPECT_TRUE(site_access_button()->GetVisible());
  EXPECT_FALSE(ExtensionsTabbedMenuView::IsShowing());

  // Click on the extensions button when the menu is closed. Extensions menu
  // should open in the installed extensions tab.
  ClickExtensionsButton();
  EXPECT_TRUE(ExtensionsTabbedMenuView::IsShowing());
  EXPECT_EQ(extensions_tabbed_menu()->GetSelectedTabIndex(), 1u);

  // Click on the extensions button when the menu is open. Extensions menu
  // should be closed.
  ClickExtensionsButton();
  EXPECT_FALSE(ExtensionsTabbedMenuView::IsShowing());

  // Click on the site access button when the menu is closed. Extensions menu
  // should open in the site access tab.
  ClickSiteAccessButton();
  EXPECT_TRUE(ExtensionsTabbedMenuView::IsShowing());
  EXPECT_EQ(extensions_tabbed_menu()->GetSelectedTabIndex(), 0u);

  // Click on the site access button when the menu is open. Extensions menu
  // should close.
  ClickSiteAccessButton();
  EXPECT_FALSE(ExtensionsTabbedMenuView::IsShowing());
}

TEST_F(ExtensionsTabbedMenuViewUnitTest, TogglingButtonsClosesMenu) {
  content::WebContentsTester* web_contents_tester =
      AddWebContentsAndGetTester();

  // Load an extension with all urls permissions so the site access button is
  // visible.
  InstallExtensionWithHostPermissions("all_urls", {"<all_urls>"});

  // Navigate to an url where the extension should have access to.
  const GURL url("http://www.a.com");
  web_contents_tester->NavigateAndCommit(url);
  WaitForAnimation();
  EXPECT_TRUE(site_access_button()->GetVisible());
  EXPECT_FALSE(ExtensionsTabbedMenuView::IsShowing());

  // Click on the extensions button when the menu is closed. Extensions menu
  // should open.
  ClickExtensionsButton();
  EXPECT_TRUE(ExtensionsTabbedMenuView::IsShowing());

  // Click on the site access button when the menu is open. Extensions menu
  // should close since the button click is treated as a click outside the menu,
  // and therefore closing the menu, instead of triggering the button's click
  // action.
  // TODO(crbug.com/1263311): Toggle to the corresponding tab when clicking on
  // the other control when the menu is open.
  ClickSiteAccessButton();
  EXPECT_FALSE(ExtensionsTabbedMenuView::IsShowing());

  // Click on the site access button when the menu is closed. Extensions menu
  // should open.
  ClickSiteAccessButton();
  EXPECT_TRUE(ExtensionsTabbedMenuView::IsShowing());

  // Click on the extensions button when the menu is open. Extensions menu
  // should close, as explained previously.
  ClickExtensionsButton();
  EXPECT_FALSE(ExtensionsTabbedMenuView::IsShowing());
}
