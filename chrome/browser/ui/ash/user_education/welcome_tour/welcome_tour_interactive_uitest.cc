// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/user_education/user_education_constants.h"
#include "ash/user_education/user_education_util.h"
#include "ash/user_education/views/help_bubble_view_ash.h"
#include "ash/user_education/welcome_tour/welcome_tour_controller.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/interaction/element_tracker_views.h"

// WelcomeTourInteractiveUiTest ------------------------------------------------

// Base class for interactive UI tests of the Welcome Tour in Ash.
class WelcomeTourInteractiveUiTest : public InteractiveBrowserTest {
 public:
  WelcomeTourInteractiveUiTest() {
    scoped_feature_list_.InitAndEnableFeature(ash::features::kWelcomeTour);

    // TODO(http://b/277091006): Remove after preventing app launches.
    // Prevent the browser from launching as it is not needed to fully exercise
    // the Welcome Tour and can only add flakiness. Eventually, logic will be
    // added to production code to prevent app launches while the Welcome Tour
    // is in progress.
    set_launch_browser_for_testing(nullptr);
  }

  // InteractiveBrowserTest:
  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();

    // Ensure that the widget context for the test interaction sequence matches
    // the initial element context used to start the Welcome Tour.
    SetContextWidget(
        views::ElementTrackerViews::GetInstance()->GetWidgetForContext(
            ash::WelcomeTourController::Get()->GetInitialElementContext()));
  }

  // Returns a builder for an interaction step that runs `steps` in the context
  // for a matching view in the primary root window for the specified
  // `element_id`. If there are multiple matches, this method does *not*
  // guarantee which context will be selected.
  [[nodiscard]] auto InMatchingViewInPrimaryRootWindowContext(
      ui::ElementIdentifier element_id,
      MultiStep steps) {
    return InContext(
        views::ElementTrackerViews::GetContextForView(
            ash::user_education_util::GetMatchingViewInRootWindow(
                display::Screen::GetScreen()->GetPrimaryDisplay().id(),
                element_id)),
        std::move(steps));
  }

  // Returns a builder for an interaction step that waits for a help bubble.
  [[nodiscard]] static auto WaitForHelpBubble() {
    return WaitForShow(ash::HelpBubbleViewAsh::kHelpBubbleElementIdForTesting);
  }

  // Returns a builder for an interaction step that checks that the body text of
  // a help bubble matches the specified `message_id`.
  [[nodiscard]] static auto CheckHelpBubbleBodyText(int message_id) {
    return CheckViewProperty(ash::HelpBubbleViewAsh::kBodyTextIdForTesting,
                             &views::Label::GetText,
                             l10n_util::GetStringUTF16(message_id));
  }

  // Returns a builder for an interaction step that checks that the default
  // button text of a help bubble matches the specified `message_id`.
  [[nodiscard]] static auto CheckHelpBubbleDefaultButtonText(int message_id) {
    return CheckViewProperty(ash::HelpBubbleViewAsh::kDefaultButtonIdForTesting,
                             &views::LabelButton::GetText,
                             l10n_util::GetStringUTF16(message_id));
  }

  // Returns a builder for an interaction step that presses the default button
  // of a help bubble.
  [[nodiscard]] auto PressHelpBubbleDefaultButton() {
    return PressButton(ash::HelpBubbleViewAsh::kDefaultButtonIdForTesting);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests -----------------------------------------------------------------------

// An interactive UI test that exercises the entire Welcome Tour.
IN_PROC_BROWSER_TEST_F(WelcomeTourInteractiveUiTest, WelcomeTour) {
  RunTestSequence(
      // Step 1: Shelf.
      WaitForHelpBubble(),
      CheckHelpBubbleBodyText(IDS_ASH_WELCOME_TOUR_SHELF_BUBBLE_BODY_TEXT),
      CheckHelpBubbleDefaultButtonText(IDS_TUTORIAL_NEXT_BUTTON),
      PressHelpBubbleDefaultButton(),

      // Step 2: Status area.
      InMatchingViewInPrimaryRootWindowContext(
          ash::kUnifiedSystemTrayElementId,
          Steps(WaitForHelpBubble(),
                CheckHelpBubbleBodyText(
                    IDS_ASH_WELCOME_TOUR_STATUS_AREA_BUBBLE_BODY_TEXT),
                CheckHelpBubbleDefaultButtonText(IDS_TUTORIAL_NEXT_BUTTON),
                PressHelpBubbleDefaultButton())),

      // Step 3: Home button.
      InMatchingViewInPrimaryRootWindowContext(
          ash::kHomeButtonElementId,
          Steps(WaitForHelpBubble(),
                CheckHelpBubbleBodyText(
                    IDS_ASH_WELCOME_TOUR_HOME_BUTTON_BUBBLE_BODY_TEXT),
                CheckHelpBubbleDefaultButtonText(IDS_TUTORIAL_NEXT_BUTTON),
                PressHelpBubbleDefaultButton()))

      // TODO(http://b:275616974): Implement after registering views.
      // Step 4: Search box.
      // Step 5: Settings app.
      // Step 6: Explore app.
  );
}
