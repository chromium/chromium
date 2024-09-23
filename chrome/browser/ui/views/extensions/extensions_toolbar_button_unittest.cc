// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"

#include "chrome/browser/ui/views/extensions/extensions_menu_coordinator.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_unittest.h"
#include "extensions/common/extension_features.h"

class ExtensionsToolbarButtonUnitTest : public ExtensionsToolbarUnitTest {
 public:
  ExtensionsToolbarButtonUnitTest();
  ~ExtensionsToolbarButtonUnitTest() override = default;
  ExtensionsToolbarButtonUnitTest(const ExtensionsToolbarButtonUnitTest&) =
      delete;
  ExtensionsToolbarButtonUnitTest& operator=(
      const ExtensionsToolbarButtonUnitTest&) = delete;

  content::WebContentsTester* web_contents_tester();
  ExtensionsMenuCoordinator* extensions_coordinator();

  void ClickExtensionsButton();

  // ExtensionsToolbarUnitTest:
  void SetUp() override;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<content::WebContentsTester, DanglingUntriaged> web_contents_tester_;
  std::unique_ptr<ExtensionsMenuCoordinator> test_extensions_coordinator_;
};

ExtensionsToolbarButtonUnitTest::ExtensionsToolbarButtonUnitTest() {
  scoped_feature_list_.InitAndEnableFeature(
      extensions_features::kExtensionsMenuAccessControl);
}

content::WebContentsTester*
ExtensionsToolbarButtonUnitTest::web_contents_tester() {
  return web_contents_tester_;
}

ExtensionsMenuCoordinator*
ExtensionsToolbarButtonUnitTest::extensions_coordinator() {
  return extensions_container()->GetExtensionsMenuCoordinatorForTesting();
}

void ExtensionsToolbarButtonUnitTest::ClickExtensionsButton() {
  ExtensionsToolbarButton* extensions_button =
      extensions_container()->GetExtensionsButton();
  ClickButton(extensions_button);
  LayoutContainerIfNecessary();
}

void ExtensionsToolbarButtonUnitTest::SetUp() {
  ExtensionsToolbarUnitTest::SetUp();
  // Menu needs web contents at construction, so we need to add them to every
  // test.
  web_contents_tester_ = AddWebContentsAndGetTester();
}

TEST_F(ExtensionsToolbarButtonUnitTest, ButtonOpensMenu) {
  InstallExtension("Extension");

  const GURL url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(url);
  WaitForAnimation();
  EXPECT_FALSE(extensions_coordinator()->IsShowing());

  ClickExtensionsButton();
  EXPECT_TRUE(extensions_coordinator()->IsShowing());

  ClickExtensionsButton();
  EXPECT_FALSE(extensions_coordinator()->IsShowing());
}

class ExtensionsToolbarButtonFeatureUnitTest
    : public ExtensionsToolbarUnitTest {
 public:
  ExtensionsToolbarButtonFeatureUnitTest() {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionsMenuAccessControl);
  }
  ~ExtensionsToolbarButtonFeatureUnitTest() override = default;
  ExtensionsToolbarButtonFeatureUnitTest(
      const ExtensionsToolbarButtonFeatureUnitTest&) = delete;
  ExtensionsToolbarButtonFeatureUnitTest& operator=(
      const ExtensionsToolbarButtonFeatureUnitTest&) = delete;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that updating the button state properly modifies the tooltip and
// accessible name.
TEST_F(ExtensionsToolbarButtonFeatureUnitTest, UpdateState) {
  InstallExtension("Extension");

  extensions_button()->UpdateState(ExtensionsToolbarButton::State::kDefault);
  EXPECT_EQ(extensions_button()->GetTooltipText({}),
            l10n_util::GetStringUTF16(IDS_TOOLTIP_EXTENSIONS_BUTTON));
  EXPECT_EQ(extensions_button()->GetViewAccessibility().GetCachedName(),
            l10n_util::GetStringUTF16(IDS_ACC_NAME_EXTENSIONS_BUTTON));

  extensions_button()->UpdateState(
      ExtensionsToolbarButton::State::kAllExtensionsBlocked);
  EXPECT_EQ(extensions_button()->GetTooltipText({}),
            l10n_util::GetStringUTF16(
                IDS_TOOLTIP_EXTENSIONS_BUTTON_ALL_EXTENSIONS_BLOCKED));
  EXPECT_EQ(extensions_button()->GetViewAccessibility().GetCachedName(),
            l10n_util::GetStringUTF16(
                IDS_ACC_NAME_EXTENSIONS_BUTTON_ALL_EXTENSIONS_BLOCKED));

  extensions_button()->UpdateState(
      ExtensionsToolbarButton::State::kAnyExtensionHasAccess);
  EXPECT_EQ(extensions_button()->GetTooltipText({}),
            l10n_util::GetStringUTF16(
                IDS_TOOLTIP_EXTENSIONS_BUTTON_ANY_EXTENSION_HAS_ACCESS));
  EXPECT_EQ(extensions_button()->GetViewAccessibility().GetCachedName(),
            l10n_util::GetStringUTF16(
                IDS_ACC_NAME_EXTENSIONS_BUTTON_ANY_EXTENSION_HAS_ACCESS));
}
