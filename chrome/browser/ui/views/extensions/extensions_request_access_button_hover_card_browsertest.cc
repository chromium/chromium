// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_request_access_button_hover_card.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/toolbar/test_toolbar_action_view_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"

class ExtensionsRequestAccessButtonHoverCardBrowserTest
    : public DialogBrowserTest {
 public:
  ExtensionsRequestAccessButtonHoverCardBrowserTest() = default;
  ExtensionsRequestAccessButtonHoverCardBrowserTest(
      const ExtensionsRequestAccessButtonHoverCardBrowserTest&) = delete;
  const ExtensionsRequestAccessButtonHoverCardBrowserTest& operator=(
      const ExtensionsRequestAccessButtonHoverCardBrowserTest&) = delete;
  ~ExtensionsRequestAccessButtonHoverCardBrowserTest() override = default;

  ExtensionsToolbarContainer* extensions_container() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->extensions_container();
  }

  ExtensionsRequestAccessButton* request_access_button() {
    return extensions_container()
        ->GetExtensionsToolbarControls()
        ->request_access_button_for_testing();
  }

  scoped_refptr<const extensions::Extension> InstallExtension() {
    scoped_refptr<const extensions::Extension> extension(
        extensions::ExtensionBuilder("Extension").Build());
    extensions::ExtensionSystem::Get(browser()->profile())
        ->extension_service()
        ->AddExtension(extension.get());
    return extension;
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    // Install extension so the extensions toolbar container, which will display
    // the request access button, is visible.
    InstallExtension();
    EXPECT_TRUE(extensions_container()->GetVisible());

    // Pretend an extension is requesting access.
    auto controllerA = std::make_unique<TestToolbarActionViewController>("A");
    std::vector<ToolbarActionViewController*> extensions_requesting_access;
    extensions_requesting_access.push_back(controllerA.get());
    extensions_container()
        ->GetExtensionsToolbarControls()
        ->UpdateRequestAccessButton(extensions_requesting_access);

    EXPECT_TRUE(request_access_button()->GetVisible());
    request_access_button()->MaybeShowHoverCard();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kExtensionsMenuAccessControl};
};

IN_PROC_BROWSER_TEST_F(ExtensionsRequestAccessButtonHoverCardBrowserTest,
                       InvokeUi) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionsRequestAccessButtonHoverCardBrowserTest,
                       InvokeUi_HoverCardVisibleOnHover) {
  EXPECT_FALSE(ExtensionsRequestAccessButtonHoverCard::IsShowing());

  ShowUi("");
  EXPECT_TRUE(ExtensionsRequestAccessButtonHoverCard::IsShowing());

  ui::MouseEvent stop_hover_event(ui::ET_MOUSE_EXITED, gfx::Point(),
                                  gfx::Point(), base::TimeTicks(), ui::EF_NONE,
                                  0);
  request_access_button()->OnMouseExited(stop_hover_event);

  EXPECT_FALSE(ExtensionsRequestAccessButtonHoverCard::IsShowing());
}
