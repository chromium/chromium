// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/organizer_panel/organizer_panel_page_handler.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_utils.h"
#include "chrome/browser/ui/webui/organizer_panel/organizer_panel.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class OrganizerPanelPageHandlerBrowserTest : public InProcessBrowserTest {
 public:
  OrganizerPanelPageHandlerBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(organizer_panel::kOrganizerPanel);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OrganizerPanelPageHandlerBrowserTest,
                       ClosePanelHidesOrganizerPanel) {
  auto* controller = OrganizerPanelController::From(browser());
  ASSERT_TRUE(controller);

  controller->SetOrganizerVisible(true);
  ASSERT_TRUE(controller->IsOrganizerPanelVisible());

  mojo::Remote<organizer_panel::mojom::PageHandler> handler_remote;
  OrganizerPanelPageHandler handler(
      handler_remote.BindNewPipeAndPassReceiver(),
      browser()->GetTabStripModel()->GetActiveWebContents());

  handler_remote->ClosePanel();
  handler_remote.FlushForTesting();

  EXPECT_FALSE(controller->IsOrganizerPanelVisible());
}

}  // namespace
