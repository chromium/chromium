// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/side_panel/read_anything/read_anything_side_panel_controller_utils.h"
#include "chrome/browser/ui/views/location_bar/read_anything_icon_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_prefs.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/events/event_utils.h"

namespace {

class ReadAnythingIconViewTest : public InProcessBrowserTest {
 public:
  ReadAnythingIconViewTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kReadAnything, features::kReadAnythingOmniboxIcon}, {});
  }

  ReadAnythingIconViewTest(const ReadAnythingIconViewTest&) = delete;
  ReadAnythingIconViewTest& operator=(const ReadAnythingIconViewTest&) = delete;
  ~ReadAnythingIconViewTest() override = default;

  PageActionIconView* GetReadAnythingOmniboxIcon() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar_button_provider()
        ->GetPageActionIconView(PageActionIconType::kReadAnything);
  }

  void SetNoDelaysForTesting() {
    SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser())
        ->SetNoDelaysForTesting(true);
  }

  void ClickReadAnythingOmniboxIcon() {
    GetReadAnythingOmniboxIcon()->button_controller()->NotifyClick();
  }

  void SetActivePageDistillable() {
    GetReadAnythingCoordinator()->ActivePageDistillableForTesting();
  }

  void SetActivePageNotDistillable() {
    GetReadAnythingCoordinator()->ActivePageNotDistillableForTesting();
  }

  int GetLabelShownCountFromPref() const {
    return browser()->profile()->GetPrefs()->GetInteger(
        prefs::kAccessibilityReadAnythingOmniboxIconLabelShownCount);
  }

 private:
  ReadAnythingCoordinator* GetReadAnythingCoordinator() {
    return ReadAnythingCoordinator::GetOrCreateForBrowser(browser());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Clicking the icon opens reading mode in the side panel.
IN_PROC_BROWSER_TEST_F(ReadAnythingIconViewTest, OpensReadingModeOnClick) {
  SetNoDelaysForTesting();
  SetActivePageDistillable();
  EXPECT_FALSE(IsReadAnythingEntryShowing(browser()));
  ClickReadAnythingOmniboxIcon();
  EXPECT_TRUE(IsReadAnythingEntryShowing(browser()));
}

// When reading mode is opened, hides the icon.
IN_PROC_BROWSER_TEST_F(ReadAnythingIconViewTest, OpenReadingModeHidesIcon) {
  SetNoDelaysForTesting();
  SetActivePageDistillable();
  PageActionIconView* icon = GetReadAnythingOmniboxIcon();
  EXPECT_TRUE(icon->GetVisible());
  ClickReadAnythingOmniboxIcon();
  EXPECT_FALSE(icon->GetVisible());
}

// When reading mode is already opened, the icon does not show.
IN_PROC_BROWSER_TEST_F(ReadAnythingIconViewTest,
                       IconNotVisibleIfReadingModeOpen) {
  SetNoDelaysForTesting();
  ShowReadAnythingSidePanel(browser(),
                            SidePanelOpenTrigger::kReadAnythingOmniboxIcon);
  SetActivePageDistillable();
  PageActionIconView* icon = GetReadAnythingOmniboxIcon();
  EXPECT_FALSE(icon->GetVisible());
}

// Show the icon when the page is distillable.
IN_PROC_BROWSER_TEST_F(ReadAnythingIconViewTest, IconShownIfDistillable) {
  PageActionIconView* icon = GetReadAnythingOmniboxIcon();
  EXPECT_FALSE(icon->GetVisible());
  SetActivePageDistillable();
  EXPECT_TRUE(icon->GetVisible());
  SetActivePageNotDistillable();
  EXPECT_FALSE(icon->GetVisible());
}

// The label only shows the first kReadAnythingOmniboxIconLabelShownCountMax
// times that the icon is shown.
IN_PROC_BROWSER_TEST_F(ReadAnythingIconViewTest, ShowLabel3Times) {
  PageActionIconView* icon = GetReadAnythingOmniboxIcon();
  for (int i = 0; i < kReadAnythingOmniboxIconLabelShownCountMax; i++) {
    EXPECT_EQ(i, GetLabelShownCountFromPref());
    SetActivePageDistillable();
    EXPECT_TRUE(icon->ShouldShowLabel());
    EXPECT_TRUE(icon->is_animating_label());
    icon->ResetSlideAnimationForTesting();
  }
  EXPECT_EQ(3, GetLabelShownCountFromPref());
  SetActivePageDistillable();
  EXPECT_FALSE(icon->ShouldShowLabel());
  EXPECT_FALSE(icon->is_animating_label());
  EXPECT_EQ(3, GetLabelShownCountFromPref());
}

}  // namespace
