// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/side_panel/lens/lens_overlay_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/lens/lens_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

using ::testing::_;

namespace {
const char kArbitraryWebUrl[] = "https://www.google.com";
}  // namespace

class LensOverlaySidePanelCoordinatorBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUp() override {
    base::test::ScopedFeatureList features;
    features.InitAndEnableFeature(lens::features::kLensOverlay);
    InProcessBrowserTest::SetUp();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  SidePanelCoordinator* side_panel_coordinator() {
    return SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser());
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(LensOverlaySidePanelCoordinatorBrowserTest,
                       ShowSidePanel) {
  auto results_coordinator =
      std::make_unique<lens::LensOverlaySidePanelCoordinator>(
          browser(), SidePanelUI::GetSidePanelUIForBrowser(browser()),
          browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kArbitraryWebUrl)));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);

  results_coordinator->RegisterEntryAndShow();
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kLensOverlayResults);
}
