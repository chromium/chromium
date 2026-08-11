// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"

#include "chrome/browser/ui/views/extensions/extensions_menu_coordinator.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_browsertest.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_features.h"
#include "ui/base/l10n/l10n_util.h"

class ExtensionsToolbarButtonBrowserTest : public ExtensionsToolbarBrowserTest {
 public:
  ExtensionsToolbarButtonBrowserTest();
  ~ExtensionsToolbarButtonBrowserTest() override = default;
  ExtensionsToolbarButtonBrowserTest(
      const ExtensionsToolbarButtonBrowserTest&) = delete;
  ExtensionsToolbarButtonBrowserTest& operator=(
      const ExtensionsToolbarButtonBrowserTest&) = delete;

  ExtensionsMenuCoordinator* extensions_coordinator();

  void ClickExtensionsButton();

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

ExtensionsToolbarButtonBrowserTest::ExtensionsToolbarButtonBrowserTest() {
  scoped_feature_list_.InitAndEnableFeature(
      extensions_features::kExtensionsMenuAccessControl);
}

ExtensionsMenuCoordinator*
ExtensionsToolbarButtonBrowserTest::extensions_coordinator() {
  return extensions_container()->GetExtensionsMenuCoordinatorForTesting();
}

void ExtensionsToolbarButtonBrowserTest::ClickExtensionsButton() {
  ExtensionsToolbarButton* extensions_button =
      extensions_container()->GetExtensionsButton();
  ClickButton(extensions_button);
  LayoutContainerIfNecessary();
}

IN_PROC_BROWSER_TEST_F(ExtensionsToolbarButtonBrowserTest, ButtonOpensMenu) {
  InstallExtension("Extension");

  const GURL url("http://www.example.com");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WaitForAnimation();
  EXPECT_FALSE(extensions_coordinator()->IsShowing());

  ClickExtensionsButton();
  EXPECT_TRUE(extensions_coordinator()->IsShowing());

  ClickExtensionsButton();
  EXPECT_FALSE(extensions_coordinator()->IsShowing());
}

// Tests that updating the button state properly modifies the tooltip and
// accessible name.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarButtonBrowserTest, UpdateState) {
  InstallExtension("Extension");

  extensions_button()->UpdateState(
      ExtensionsToolbarViewModel::ExtensionsToolbarButtonState::kDefault);
  EXPECT_EQ(extensions_button()->GetRenderedTooltipText({}),
            l10n_util::GetStringUTF16(IDS_TOOLTIP_EXTENSIONS_BUTTON));
  EXPECT_EQ(extensions_button()->GetViewAccessibility().GetCachedName(),
            l10n_util::GetStringUTF16(IDS_ACC_NAME_EXTENSIONS_BUTTON));

  extensions_button()->UpdateState(
      ExtensionsToolbarViewModel::ExtensionsToolbarButtonState::
          kAllExtensionsBlocked);
  EXPECT_EQ(extensions_button()->GetRenderedTooltipText({}),
            l10n_util::GetStringUTF16(
                IDS_TOOLTIP_EXTENSIONS_BUTTON_ALL_EXTENSIONS_BLOCKED));
  EXPECT_EQ(extensions_button()->GetViewAccessibility().GetCachedName(),
            l10n_util::GetStringUTF16(
                IDS_ACC_NAME_EXTENSIONS_BUTTON_ALL_EXTENSIONS_BLOCKED));

  extensions_button()->UpdateState(
      ExtensionsToolbarViewModel::ExtensionsToolbarButtonState::
          kAnyExtensionHasAccess);
  EXPECT_EQ(extensions_button()->GetRenderedTooltipText({}),
            l10n_util::GetStringUTF16(
                IDS_TOOLTIP_EXTENSIONS_BUTTON_ANY_EXTENSION_HAS_ACCESS));
  EXPECT_EQ(extensions_button()->GetViewAccessibility().GetCachedName(),
            l10n_util::GetStringUTF16(
                IDS_ACC_NAME_EXTENSIONS_BUTTON_ANY_EXTENSION_HAS_ACCESS));
}
