// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_request_access_hover_card_coordinator.h"

#include "chrome/browser/ui/views/extensions/extensions_dialogs_browsertest.h"
#include "chrome/browser/ui/views/extensions/extensions_request_access_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"

class ExtensionsRequestAccessHoverCardCoordinatorBrowserTest
    : public ExtensionsDialogBrowserTest {
 public:
  ExtensionsRequestAccessButton* request_access_button() {
    return extensions_container()->GetRequestAccessButton();
  }

  ExtensionsRequestAccessHoverCardCoordinator* hover_card_coordinator() {
    return request_access_button()->GetHoverCardCoordinatorForTesting();
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    // Install extension so the extensions toolbar container, which will display
    // the request access button, is visible.
    auto extension = InstallExtension("Extension");
    EXPECT_TRUE(extensions_container()->GetVisible());

    // Pretend an extension is requesting access.
    std::vector<extensions::ExtensionId> extension_ids = {extension->id()};
    request_access_button()->Update(extension_ids);
    request_access_button()->SetVisible(true);

    request_access_button()->MaybeShowHoverCard();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      extensions_features::kExtensionsMenuAccessControl};
};

IN_PROC_BROWSER_TEST_F(ExtensionsRequestAccessHoverCardCoordinatorBrowserTest,
                       InvokeUi) {
  ShowAndVerifyUi();
}

// TODO(crbug.com/40879945): Disabled because we are showing a tooltip instead
// of hover card. Remove once kExtensionsMenuAccessControlWithPermittedSites is
// rolled out. We are keeping it for now since we may bring the hover card back.
IN_PROC_BROWSER_TEST_F(ExtensionsRequestAccessHoverCardCoordinatorBrowserTest,
                       DISABLED_InvokeUi_HoverCardVisibleOnHover) {
  EXPECT_FALSE(hover_card_coordinator()->IsShowing());

  ShowUi("");
  EXPECT_TRUE(hover_card_coordinator()->IsShowing());

  ui::MouseEvent stop_hover_event(ui::EventType::kMouseExited, gfx::Point(),
                                  gfx::Point(), base::TimeTicks(), ui::EF_NONE,
                                  0);
  request_access_button()->OnMouseExited(stop_hover_event);

  EXPECT_FALSE(hover_card_coordinator()->IsShowing());
}
