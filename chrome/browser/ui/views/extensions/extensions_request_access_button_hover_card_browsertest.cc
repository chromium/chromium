// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_request_access_button_hover_card.h"

#include "chrome/browser/ui/toolbar/test_toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_dialogs_browsertest.h"
#include "chrome/browser/ui/views/extensions/extensions_request_access_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"

class ExtensionsRequestAccessButtonHoverCardBrowserTest
    : public ExtensionsDialogBrowserTest {
 public:
  ExtensionsRequestAccessButton* request_access_button() {
    return extensions_container()
        ->GetExtensionsToolbarControls()
        ->request_access_button_for_testing();
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    // Install extension so the extensions toolbar container, which will display
    // the request access button, is visible.
    InstallExtension("Extension");
    EXPECT_TRUE(extensions_container()->GetVisible());

    // Pretend an extension is requesting access.
    auto controllerA = std::make_unique<TestToolbarActionViewController>("A");
    std::vector<ToolbarActionViewController*> extensions_requesting_access;
    extensions_requesting_access.push_back(controllerA.get());
    request_access_button()->UpdateExtensionsRequestingAccess(
        extensions_requesting_access);
    request_access_button()->SetVisible(true);

    request_access_button()->MaybeShowHoverCard();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      extensions_features::kExtensionsMenuAccessControl};
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
