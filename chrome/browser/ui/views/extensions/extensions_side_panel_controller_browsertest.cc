// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/extensions/extensions_side_panel_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"

namespace {

const char kTargetedId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kSidePanelResourceName[] = "side_panel.html";
const char kPanelActiveKey[] = "active";
const char kPanelActivatableKey[] = "activatable";
const char kPanelWidth[] = "width";
const char kTrue[] = "true";
const char kFalse[] = "false";

std::string GetResourceString(const std::string& key,
                              const std::string& value) {
  return base::StrCat({kSidePanelResourceName, "?", key, "=", value});
}

}  // namespace

class ExtensionsSidePanelControllerBrowserTest : public InProcessBrowserTest {
 public:
  ExtensionsSidePanelControllerBrowserTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        {features::kExtensionsSidePanel},
        {{"ExtensionsSidePanelId", kTargetedId}});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    extension_ = extensions::ExtensionBuilder("foo").SetID(kTargetedId).Build();
    GetExtensionService()->AddExtension(extension_.get());

    // The following should exist when the `kExtensionsSidePanel` feature flag
    // is enabled and the extension with id specified by `ExtensionsSidePanelId`
    // is present.
    ASSERT_NE(nullptr, side_panel());
    ASSERT_NE(nullptr, side_panel_button());
    ASSERT_NE(nullptr, controller());
  }

  extensions::ExtensionService* GetExtensionService() {
    return extensions::ExtensionSystem::Get(browser()->profile())
        ->extension_service();
  }

  BrowserView* browser_view() {
    return static_cast<BrowserView*>(browser()->window());
  }

  SidePanel* side_panel() {
    return browser_view()->left_aligned_side_panel_for_testing();
  }

  ToolbarButton* side_panel_button() {
    return browser_view()->toolbar()->left_side_panel_button();
  }

  ExtensionsSidePanelController* controller() {
    return browser_view()->extensions_side_panel_controller();
  }

  const extensions::Extension* extension() { return extension_.get(); }

 private:
  scoped_refptr<const extensions::Extension> extension_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ExtensionsSidePanelControllerBrowserTest,
                       LeftSidePanelButtonVisibleOnlyWhenExtensionEnabled) {
  EXPECT_TRUE(side_panel_button()->GetVisible());

  // Remove the extension.
  GetExtensionService()->UnloadExtension(
      kTargetedId, extensions::UnloadedExtensionReason::UNINSTALL);
  EXPECT_FALSE(side_panel_button()->GetVisible());

  // Reenable the extension.
  GetExtensionService()->AddExtension(extension());
  EXPECT_TRUE(side_panel_button()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(ExtensionsSidePanelControllerBrowserTest,
                       ToggleSidePanelViaURLParams) {
  EXPECT_FALSE(side_panel()->GetVisible());

  // Toggle the side panel into the visible state.
  controller()->get_web_view_for_testing()->LoadInitialURL(
      extension()->GetResourceURL(GetResourceString(kPanelActiveKey, kTrue)));
  content::WaitForLoadStop(
      controller()->get_web_view_for_testing()->GetWebContents());
  EXPECT_TRUE(side_panel()->GetVisible());

  // Hide the side panel.
  controller()->get_web_view_for_testing()->LoadInitialURL(
      extension()->GetResourceURL(GetResourceString(kPanelActiveKey, kFalse)));
  content::WaitForLoadStop(
      controller()->get_web_view_for_testing()->GetWebContents());
  EXPECT_FALSE(side_panel()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(ExtensionsSidePanelControllerBrowserTest,
                       SetSidePanelWidthViaURLParams) {
  constexpr int kWidth1 = 300;
  controller()->get_web_view_for_testing()->LoadInitialURL(
      extension()->GetResourceURL(
          GetResourceString(kPanelWidth, base::NumberToString(kWidth1))));
  content::WaitForLoadStop(
      controller()->get_web_view_for_testing()->GetWebContents());
  EXPECT_EQ(side_panel()->GetPreferredSize().width(), kWidth1);

  constexpr int kWidth2 = 200;
  controller()->get_web_view_for_testing()->LoadInitialURL(
      extension()->GetResourceURL(
          GetResourceString(kPanelWidth, base::NumberToString(kWidth2))));
  content::WaitForLoadStop(
      controller()->get_web_view_for_testing()->GetWebContents());
  EXPECT_EQ(side_panel()->GetPreferredSize().width(), kWidth2);
}

IN_PROC_BROWSER_TEST_F(ExtensionsSidePanelControllerBrowserTest,
                       SetSidePanelActivatableViaURLParams) {
  EXPECT_FALSE(side_panel_button()->GetEnabled());

  // Set the side panel to be activatable, enabling the side panel button.
  controller()->get_web_view_for_testing()->LoadInitialURL(
      extension()->GetResourceURL(
          GetResourceString(kPanelActivatableKey, kTrue)));
  content::WaitForLoadStop(
      controller()->get_web_view_for_testing()->GetWebContents());
  EXPECT_TRUE(side_panel_button()->GetEnabled());

  // Set activatable to false, disabling the side panel button.
  controller()->get_web_view_for_testing()->LoadInitialURL(
      extension()->GetResourceURL(
          GetResourceString(kPanelActivatableKey, kFalse)));
  content::WaitForLoadStop(
      controller()->get_web_view_for_testing()->GetWebContents());
  EXPECT_FALSE(side_panel_button()->GetEnabled());
}
