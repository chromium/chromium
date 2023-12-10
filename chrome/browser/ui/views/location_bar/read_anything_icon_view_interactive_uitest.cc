// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/read_anything_icon_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/side_panel/read_anything/read_anything_side_panel_controller_utils.h"
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

  void ClickReadAnythingOmniboxIcon() {
    GetReadAnythingOmniboxIcon()->button_controller()->NotifyClick();
  }

  void SetActivePageDistillable() {
    GetReadAnythingCoordinator()->ActivePageDistillableForTesting();
  }

  void SetActivePageNotDistillable() {
    GetReadAnythingCoordinator()->ActivePageNotDistillableForTesting();
  }

 private:
  ReadAnythingCoordinator* GetReadAnythingCoordinator() {
    return ReadAnythingCoordinator::GetOrCreateForBrowser(browser());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Clicking the icon opens reading mode in the side panel.
IN_PROC_BROWSER_TEST_F(ReadAnythingIconViewTest, OpensReadingModeOnClick) {
  SetActivePageDistillable();
  EXPECT_FALSE(IsReadAnythingEntryShowing(browser()));
  ClickReadAnythingOmniboxIcon();
  EXPECT_TRUE(IsReadAnythingEntryShowing(browser()));
}

// When reading mode is opened, hides the icon.
IN_PROC_BROWSER_TEST_F(ReadAnythingIconViewTest, OpenReadingModeHidesIcon) {
  SetActivePageDistillable();
  PageActionIconView* icon = GetReadAnythingOmniboxIcon();
  EXPECT_TRUE(icon->GetVisible());
  ClickReadAnythingOmniboxIcon();
  EXPECT_FALSE(icon->GetVisible());
}

// When reading mode is already opened, the icon does not show.
IN_PROC_BROWSER_TEST_F(ReadAnythingIconViewTest,
                       IconNotVisibleIfReadingModeOpen) {
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

// The label only shows the first 3 times that the icon is shown.
IN_PROC_BROWSER_TEST_F(ReadAnythingIconViewTest, ShowLabel3Times) {
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kAccessibilityReadAnythingOmniboxIconLabelShownCount, 0);

  PageActionIconView* icon = GetReadAnythingOmniboxIcon();
  for (int i = 0; i < 3; i++) {
    EXPECT_TRUE(icon->ShouldShowLabel());
    SetActivePageDistillable();
  }
  EXPECT_FALSE(icon->ShouldShowLabel());
}

}  // namespace
