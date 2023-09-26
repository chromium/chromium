// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_element_identifiers.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/pill_button.h"
#include "ash/style/system_dialog_delegate_view.h"
#include "ash/user_education/views/help_bubble_view_ash.h"
#include "ash/user_education/welcome_tour/welcome_tour_controller.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {

// Aliases.
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Matches;
using ::testing::Property;

// Matchers --------------------------------------------------------------------

MATCHER_P(ElementIdentifier, matcher, "") {
  return Matches(matcher)(arg->GetProperty(views::kElementIdentifierKey));
}

MATCHER_P(RootWindow, matcher, "") {
  return Matches(matcher)(arg->GetWidget()->GetNativeWindow()->GetRootWindow());
}

}  // namespace

// WelcomeTourInteractiveUiTest ------------------------------------------------

// Base class for interactive UI tests of the Welcome Tour in Ash.
class WelcomeTourInteractiveUiTest : public InteractiveBrowserTest {
 public:
  WelcomeTourInteractiveUiTest() {
    // NOTE: These tests are not concerned with user eligibility, so explicitly
    // force user eligibility for the Welcome Tour.
    scoped_feature_list_.InitWithFeatures(
        {ash::features::kWelcomeTour,
         ash::features::kWelcomeTourForceUserEligibility},
        {});

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

    // Install system apps.
    // NOTE: This test requires the "Help" and "Settings" apps to be installed.
    Profile* const profile = ProfileManager::GetActiveUserProfile();
    web_app::test::WaitUntilReady(web_app::WebAppProvider::GetForTest(profile));
    ash::SystemWebAppManager::GetForTest(profile)
        ->InstallSystemAppsForTesting();
    AppListClientImpl::GetInstance()->UpdateProfile();

    // Ensure that the widget context for the test interaction sequence matches
    // the initial element context used to start the Welcome Tour.
    SetContextWidget(
        views::ElementTrackerViews::GetInstance()->GetWidgetForContext(
            ash::WelcomeTourController::Get()->GetInitialElementContext()));
  }

  // Returns a builder for an interaction step that waits for the app list
  // bubble to hide.
  [[nodiscard]] static auto WaitForAppListBubbleToHide() {
    return WaitForHide(ash::HelpBubbleViewAsh::kHelpBubbleElementIdForTesting);
  }

  // Returns a builder for an interaction step that waits for the browser.
  [[nodiscard]] static auto WaitForBrowser() {
    return WaitForShow(kBrowserViewElementId);
  }

  // Returns a builder for an interaction step that waits for the dialog to have
  // the expected visibility.
  [[nodiscard]] static auto WaitForDialogVisibility(bool visible) {
    return visible ? WaitForShow(ash::kWelcomeTourDialogElementId)
                   : WaitForHide(ash::kWelcomeTourDialogElementId);
  }

  // Returns a builder for an interaction step that waits for a help bubble.
  [[nodiscard]] static auto WaitForHelpBubble() {
    return WaitForShow(ash::HelpBubbleViewAsh::kHelpBubbleElementIdForTesting);
  }

  // Returns a builder for an interaction step that waits for login user view.
  [[nodiscard]] static auto WaitForLoginUserView() {
    return WaitForShow(ash::kLoginUserViewElementId);
  }

  // Returns a builder for an interaction step that checks the visibility of
  // app list bubble.
  [[nodiscard]] static auto CheckAppListBubbleVisibility(bool visible) {
    return CheckViewProperty(ash::kAppListBubbleViewElementId,
                             &views::View::GetVisible, visible);
  }

  // Returns a builder for an interaction step that checks the browser is
  // for a web app associated with the specified `app_id`.
  [[nodiscard]] static auto CheckBrowserIsForWebApp(
      const webapps::AppId& app_id) {
    return CheckView(kBrowserViewElementId,
                     [app_id](BrowserView* browser_view) {
                       return web_app::AppBrowserController::IsForWebApp(
                           browser_view->browser(), app_id);
                     });
  }

  // Returns a builder for an interaction step that checks whether the dialog
  // accept button is focused.
  [[nodiscard]] static auto CheckDialogAcceptButtonFocus(bool focused) {
    return CheckViewProperty(
        ash::SystemDialogDelegateView::kAcceptButtonIdForTesting,
        &views::MdTextButton::HasFocus, focused);
  }

  // Returns a builder for an interaction step that checks the dialog accept
  // button text.
  [[nodiscard]] static auto CheckDialogAcceptButtonText() {
    return CheckViewProperty(
        ash::SystemDialogDelegateView::kAcceptButtonIdForTesting,
        &ash::PillButton::GetText,
        l10n_util::GetStringUTF16(
            IDS_ASH_WELCOME_TOUR_DIALOG_ACCEPT_BUTTON_TEXT));
  }

  // Returns a builder for an interaction step that checks the dialog cancel
  // button text.
  [[nodiscard]] static auto CheckDialogCancelButtonText() {
    return CheckViewProperty(
        ash::SystemDialogDelegateView::kCancelButtonIdForTesting,
        &ash::PillButton::GetText,
        l10n_util::GetStringUTF16(
            IDS_ASH_WELCOME_TOUR_DIALOG_CANCEL_BUTTON_TEXT));
  }

  // Returns a builder for an interaction step that checks the dialog
  // description.
  [[nodiscard]] static auto CheckDialogDescription() {
    return CheckViewProperty(
        ash::SystemDialogDelegateView::kDescriptionTextIdForTesting,
        &views::Label::GetText,
        l10n_util::GetStringUTF16(
            IDS_ASH_WELCOME_TOUR_DIALOG_DESCRIPTION_TEXT));
  }

  // Returns a builder for an interaction step that checks the dialog title.
  [[nodiscard]] static auto CheckDialogTitle() {
    return CheckViewProperty(
        ash::SystemDialogDelegateView::kTitleTextIdForTesting,
        &views::Label::GetText,
        l10n_util::GetStringUTF16(IDS_ASH_WELCOME_TOUR_DIALOG_TITLE_TEXT));
  }

  // Returns a builder for an interaction step that checks that the anchor of a
  // help bubble (a) matches the specified `element_id`, and (b) is contained
  // within the primary root window.
  [[nodiscard]] static auto CheckHelpBubbleAnchor(
      ui::ElementIdentifier element_id) {
    return CheckViewProperty<ash::HelpBubbleViewAsh, views::View*>(
        ash::HelpBubbleViewAsh::kHelpBubbleElementIdForTesting,
        &ash::HelpBubbleViewAsh::GetAnchorView,
        AllOf(ElementIdentifier(Eq(element_id)),
              RootWindow(Eq(ash::Shell::GetPrimaryRootWindow()))));
  }

  // Returns a builder for an interaction step that checks that the body text of
  // a help bubble matches the specified `message_id`.
  [[nodiscard]] static auto CheckHelpBubbleBodyText(int message_id) {
    return CheckViewProperty(ash::HelpBubbleViewAsh::kBodyTextIdForTesting,
                             &views::Label::GetText,
                             l10n_util::GetStringUTF16(message_id));
  }

  // Returns a builder for an interaction step that checks whether the help
  // bubble default button is focused.
  [[nodiscard]] static auto CheckHelpBubbleDefaultButtonFocus(bool focused) {
    return CheckViewProperty(ash::HelpBubbleViewAsh::kDefaultButtonIdForTesting,
                             &views::MdTextButton::HasFocus, focused);
  }

  // Returns a builder for an interaction step that checks that the default
  // button text of a help bubble matches the specified `message_id`.
  [[nodiscard]] static auto CheckHelpBubbleDefaultButtonText(int message_id) {
    return CheckViewProperty(ash::HelpBubbleViewAsh::kDefaultButtonIdForTesting,
                             &views::LabelButton::GetText,
                             l10n_util::GetStringUTF16(message_id));
  }

  // Returns a builder for an interaction step that presses the dialog accept
  // button.
  [[nodiscard]] auto PressDialogAcceptButton() {
    return PressButton(
        ash::SystemDialogDelegateView::kAcceptButtonIdForTesting);
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
      // Step 0: Dialog.
      InAnyContext(WaitForDialogVisibility(true)),
      InSameContext(Steps(
          CheckDialogAcceptButtonFocus(true), CheckDialogAcceptButtonText(),
          CheckDialogCancelButtonText(), CheckDialogDescription(),
          CheckDialogTitle(), PressDialogAcceptButton(), FlushEvents())),

      // Step 1: Shelf.
      InAnyContext(WaitForHelpBubble()),
      InSameContext(Steps(
          CheckHelpBubbleAnchor(ash::kShelfViewElementId),
          CheckHelpBubbleBodyText(IDS_ASH_WELCOME_TOUR_SHELF_BUBBLE_BODY_TEXT),
          CheckHelpBubbleDefaultButtonFocus(true),
          CheckHelpBubbleDefaultButtonText(IDS_TUTORIAL_NEXT_BUTTON),
          PressHelpBubbleDefaultButton(), FlushEvents())),

      // Step 2: Status area.
      InAnyContext(WaitForHelpBubble()),
      InSameContext(
          Steps(CheckHelpBubbleAnchor(ash::kUnifiedSystemTrayElementId),
                CheckHelpBubbleBodyText(
                    IDS_ASH_WELCOME_TOUR_STATUS_AREA_BUBBLE_BODY_TEXT),
                CheckHelpBubbleDefaultButtonFocus(true),
                CheckHelpBubbleDefaultButtonText(IDS_TUTORIAL_NEXT_BUTTON),
                PressHelpBubbleDefaultButton(), FlushEvents())),

      // Step 3: Home button.
      InAnyContext(WaitForHelpBubble()),
      InSameContext(
          Steps(CheckHelpBubbleAnchor(ash::kHomeButtonElementId),
                CheckHelpBubbleBodyText(
                    IDS_ASH_WELCOME_TOUR_HOME_BUTTON_BUBBLE_BODY_TEXT),
                CheckHelpBubbleDefaultButtonFocus(true),
                CheckHelpBubbleDefaultButtonText(IDS_TUTORIAL_NEXT_BUTTON),
                PressHelpBubbleDefaultButton(), FlushEvents())),

      // Step 4: Search box.
      InAnyContext(WaitForHelpBubble()),
      InSameContext(
          Steps(CheckAppListBubbleVisibility(true),
                CheckHelpBubbleAnchor(ash::kSearchBoxViewElementId),
                CheckHelpBubbleBodyText(
                    IDS_ASH_WELCOME_TOUR_SEARCH_BOX_BUBBLE_BODY_TEXT),
                CheckHelpBubbleDefaultButtonFocus(true),
                CheckHelpBubbleDefaultButtonText(IDS_TUTORIAL_NEXT_BUTTON),
                PressHelpBubbleDefaultButton(), FlushEvents())),

      // Step 5: Settings app.
      InAnyContext(WaitForHelpBubble()),
      InSameContext(
          Steps(CheckAppListBubbleVisibility(true),
                CheckHelpBubbleAnchor(ash::kSettingsAppElementId),
                CheckHelpBubbleBodyText(
                    IDS_ASH_WELCOME_TOUR_SETTINGS_APP_BUBBLE_BODY_TEXT),
                CheckHelpBubbleDefaultButtonFocus(true),
                CheckHelpBubbleDefaultButtonText(IDS_TUTORIAL_NEXT_BUTTON),
                PressHelpBubbleDefaultButton(), FlushEvents())),

      // Step 6: Explore app.
      InAnyContext(WaitForHelpBubble()),
      InSameContext(
          Steps(CheckAppListBubbleVisibility(true),
                CheckHelpBubbleAnchor(ash::kExploreAppElementId),
                CheckHelpBubbleBodyText(
                    IDS_ASH_WELCOME_TOUR_EXPLORE_APP_BUBBLE_BODY_TEXT),
                CheckHelpBubbleDefaultButtonFocus(true),
                CheckHelpBubbleDefaultButtonText(
                    IDS_ASH_WELCOME_TOUR_COMPLETE_BUTTON_TEXT),
                PressHelpBubbleDefaultButton(), FlushEvents())),

      // Step 7: Explore app window.
      InAnyContext(WaitForBrowser()),
      InSameContext(Steps(WaitForAppListBubbleToHide(),
                          CheckBrowserIsForWebApp(web_app::kHelpAppId))));
}

// An interactive UI test that locks the screen during the Welcome Tour.
IN_PROC_BROWSER_TEST_F(WelcomeTourInteractiveUiTest,
                       LockScreenDuringWelcomeTour) {
  RunTestSequence(
      // Wait for the Welcome Tour dialog to show.
      InAnyContext(WaitForDialogVisibility(true)),

      InSameContext(Steps(
          // Lock screen through accelerator.
          Do([]() {
            ui::test::EventGenerator(ash::Shell::GetPrimaryRootWindow())
                .PressAndReleaseKey(ui::KeyboardCode::VKEY_L,
                                    ui::EF_COMMAND_DOWN);
          }),

          // Wait for the Welcome Tour dialog to hide.
          WaitForDialogVisibility(false))),

      // Wait for the login user view to show.
      InAnyContext(WaitForLoginUserView()));
}
