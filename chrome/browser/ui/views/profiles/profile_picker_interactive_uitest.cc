// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_view.h"

#include "base/check.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/views/profiles/profile_picker_interactive_uitest_base.h"
#include "chrome/browser/ui/views/profiles/profile_picker_test_base.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

using testing::Eq;
using testing::IsFalse;
using testing::IsTrue;

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kProfilePickerViewId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPickerWebContentsId);

const WebContentsInteractionTestUtil::DeepQuery kSignInButton = {
    "profile-picker-app", "profile-type-choice", "#signInButton"};
const WebContentsInteractionTestUtil::DeepQuery kAddProfileButton = {
    "profile-picker-app", "profile-picker-main-view", "#addProfile"};

// Waits until the widget bounds change.
class WidgetBoundsChangeWaiter : public views::WidgetObserver {
 public:
  explicit WidgetBoundsChangeWaiter(views::Widget* widget) {
    DCHECK(widget);
    observation_.Observe(widget);
  }

  // Waits until the widget bounds change.
  void Wait() { run_loop_.Run(); }

 private:
  // WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override {
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  base::ScopedObservation<views::Widget, views::WidgetObserver> observation_{
      this};
};

}  // namespace

struct NavState {
  int entry_count;
  int last_committed_entry_index;
  bool has_pending_entry = false;

  bool operator==(const NavState& other) const {
    return entry_count == other.entry_count &&
           last_committed_entry_index == other.last_committed_entry_index &&
           has_pending_entry == other.has_pending_entry;
  }

  friend std::ostream& operator<<(std::ostream& stream, const NavState& state) {
    return stream << "{entry_count=" << state.entry_count
                  << ", last_committed_entry_index="
                  << state.last_committed_entry_index
                  << ", has_pending_entry=" << state.has_pending_entry << "}";
  }
};

class ProfilePickerInteractiveUiTest
    : public InteractiveBrowserTest,
      public WithProfilePickerInteractiveUiTestHelpers {
 public:
  ProfilePickerInteractiveUiTest() = default;
  ~ProfilePickerInteractiveUiTest() override = default;

  void ShowAndFocusPicker(ProfilePicker::EntryPoint entry_point,
                          const GURL& expected_url = GURL()) {
    ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(entry_point));
    WaitForPickerWidgetCreated();
    view()->SetProperty(views::kElementIdentifierKey, kProfilePickerViewId);
    if (!expected_url.is_empty()) {
      WaitForLoadStop(expected_url);
    }

    EXPECT_TRUE(
        ui_test_utils::ShowAndFocusNativeWindow(widget()->GetNativeWindow()));
  }

  auto GetNavState() {
    return [this]() {
      return NavState{
          .entry_count = web_contents()->GetController().GetEntryCount(),
          .last_committed_entry_index =
              web_contents()->GetController().GetLastCommittedEntryIndex(),
          .has_pending_entry =
              web_contents()->GetController().GetPendingEntry() != nullptr,
      };
    };
  }

  auto HasPendingNav() {
    return [this]() {
      return web_contents()->GetController().GetPendingEntry() != nullptr;
    };
  }

  void SimulateUserActivation() {
    content::UpdateUserActivationStateInterceptor user_activation_interceptor(
        web_contents()->GetPrimaryMainFrame());
    user_activation_interceptor.UpdateUserActivationState(
        blink::mojom::UserActivationUpdateType::kNotifyActivation,
        blink::mojom::UserActivationNotificationType::kTest);
  }

  StateChange Exists(const DeepQuery& where) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementExistsEvent);
    StateChange state_change;
    state_change.type = StateChange::Type::kExists;
    state_change.where = where;
    state_change.event = kElementExistsEvent;
    return state_change;
  }

  // Used to wait for screen changes inside of the `profile-picker-app`, in
  // combination with `WaitForStateChange()`.
  //
  // In the profile picker app we modify the displayed URL but don't do a full
  // navigation, so `WaitForWebContentsNavigation()` does not detect these
  // changes.
  StateChange UrlEntryMatches(const GURL& url) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kUrlEntryMatchesEvent);
    StateChange state_change;
    state_change.test_function = base::StringPrintf(
        "(_) => navigation && navigation.currentEntry && "
        "navigation.currentEntry.url === '%s'",
        url.spec().c_str());
    state_change.type = StateChange::Type::kConditionTrue;
    state_change.event = kUrlEntryMatchesEvent;
    return state_change;
  }
};

// Checks that the main picker view can be closed with keyboard shortcut.
IN_PROC_BROWSER_TEST_F(ProfilePickerInteractiveUiTest, CloseWithKeyboard) {
  // Open a new picker.
  ShowAndFocusPicker(ProfilePicker::EntryPoint::kProfileMenuManageProfiles,
                     GURL("chrome://profile-picker"));
  EXPECT_TRUE(ProfilePicker::IsOpen());
  SendCloseWindowKeyboardCommand();
  WaitForPickerClosed();
  // Closing the picker does not exit Chrome.
  EXPECT_FALSE(browser_shutdown::IsTryingToQuit());
}

#if BUILDFLAG(IS_MAC)
// Checks that Chrome be closed with keyboard shortcut. Only MacOS has a
// keyboard shortcut to exit Chrome.
IN_PROC_BROWSER_TEST_F(ProfilePickerInteractiveUiTest, ExitWithKeyboard) {
  // Open a new picker.
  ShowAndFocusPicker(ProfilePicker::EntryPoint::kProfileMenuManageProfiles,
                     GURL("chrome://profile-picker"));
  EXPECT_TRUE(ProfilePicker::IsOpen());

  SendQuitAppKeyboardCommand();
  WaitForPickerClosed();
  EXPECT_TRUE(browser_shutdown::IsTryingToQuit());
}
#endif

// Checks that the main picker view can switch to full screen.
IN_PROC_BROWSER_TEST_F(ProfilePickerInteractiveUiTest, FullscreenWithKeyboard) {
  // Open a new picker.
  ShowAndFocusPicker(ProfilePicker::EntryPoint::kProfileMenuManageProfiles,
                     GURL("chrome://profile-picker"));
  EXPECT_TRUE(ProfilePicker::IsOpen());
  EXPECT_FALSE(widget()->IsFullscreen());
  WidgetBoundsChangeWaiter bounds_waiter(widget());

  SendToggleFullscreenKeyboardCommand();

  // Fullscreen causes the bounds of the widget to change.
  bounds_waiter.Wait();
  EXPECT_TRUE(widget()->IsFullscreen());
}

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(ProfilePickerInteractiveUiTest,
                       CloseDiceSigninWithKeyboard) {
  ShowAndFocusPicker(ProfilePicker::EntryPoint::kProfileMenuAddNewProfile);

  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),

      // Wait for the profile picker to show the profile type choice screen.
      WaitForShow(kProfilePickerViewId),
      InstrumentNonTabWebView(kPickerWebContentsId, web_view()),
      WaitForWebContentsReady(kPickerWebContentsId,
                              GURL("chrome://profile-picker/new-profile")),

      // Click "sign in".
      // Note: the button should be disabled after this, but there is no good
      // way to verify it in this sequence. It is verified by unit tests in
      // chrome/test/data/webui/signin/profile_picker_app_test.ts
      EnsurePresent(kPickerWebContentsId, kSignInButton),
      MoveMouseTo(kPickerWebContentsId, kSignInButton), ClickMouse(),

      // Wait for switch to the Gaia sign-in page to complete.
      // Note: kPickerWebContentsId now points to the new profile's WebContents.
      WaitForWebContentsNavigation(kPickerWebContentsId,
                                   GetSigninChromeSyncDiceUrl()),

      // Send "Close window" keyboard shortcut and wait for view to close.
      SendAccelerator(kProfilePickerViewId, GetAccelerator(IDC_CLOSE_WINDOW)),
      WaitForHide(kProfilePickerViewId, /*transition_only_on_event=*/true),

      // Note: The widget/view is destroyed asynchronously, we need to flush the
      // message loops to be able to reliably check the global state.
      FlushEvents(), CheckResult(&ProfilePicker::IsOpen, testing::IsFalse()));
}

// Checks that both the signin web view and the main picker view are able to
// process a back keyboard event.
IN_PROC_BROWSER_TEST_F(ProfilePickerInteractiveUiTest,
                       NavigateBackFromDiceSigninWithKeyboard) {
  // Simulate walking through the flow starting at the picker so that navigating
  // back to the picker makes sense. Check that the navigation list is populated
  // correctly.
  ShowAndFocusPicker(ProfilePicker::EntryPoint::kProfileMenuManageProfiles,
                     GURL("chrome://profile-picker"));

  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),

      WaitForShow(kProfilePickerViewId),
      InstrumentNonTabWebView(kPickerWebContentsId, web_view()),
      WaitForWebContentsReady(kPickerWebContentsId,
                              GURL("chrome://profile-picker")),
      CheckResult(GetNavState(), Eq(NavState{.entry_count = 1,
                                             .last_committed_entry_index = 0})),

      // Advance to the profile type choice screen.
      EnsurePresent(kPickerWebContentsId, kAddProfileButton),
      MoveMouseTo(kPickerWebContentsId, kAddProfileButton), ClickMouse(),
      WaitForStateChange(
          kPickerWebContentsId,
          UrlEntryMatches(GURL("chrome://profile-picker/new-profile"))),
      CheckResult(GetNavState(), Eq(NavState{.entry_count = 2,
                                             .last_committed_entry_index = 1})),

      // Advance to the sign-in page.
      WaitForStateChange(kPickerWebContentsId, Exists(kSignInButton)),
      MoveMouseTo(kPickerWebContentsId, kSignInButton), ClickMouse(),
      WaitForWebContentsNavigation(kPickerWebContentsId,
                                   GetSigninChromeSyncDiceUrl()),

      // Navigate back with the keyboard, switch back to the picker.
      SendAccelerator(kProfilePickerViewId, GetAccelerator(IDC_BACK)),
      WaitForStateChange(
          kPickerWebContentsId,
          UrlEntryMatches(GURL("chrome://profile-picker/new-profile"))),
      CheckResult(GetNavState(), Eq(NavState{.entry_count = 2,
                                             .last_committed_entry_index = 1})),

      // Navigate again back with the keyboard.
      SendAccelerator(kProfilePickerViewId, GetAccelerator(IDC_BACK)),
      CheckResult(HasPendingNav(), IsTrue()),
      WaitForStateChange(kPickerWebContentsId,
                         UrlEntryMatches(GURL("chrome://profile-picker"))),
      CheckResult(GetNavState(), Eq(NavState{.entry_count = 2,
                                             .last_committed_entry_index = 0})),

      // Navigating back once again does nothing.
      SendAccelerator(kProfilePickerViewId, GetAccelerator(IDC_BACK)),
      CheckResult(HasPendingNav(), IsFalse()));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

IN_PROC_BROWSER_TEST_F(ProfilePickerInteractiveUiTest,
                       NavigateBackFromProfileTypeChoiceWithKeyboard) {
  // Simulate walking through the flow starting at the picker so that navigating
  // back to the picker makes sense. Check that the navigation list is populated
  // correctly.
  ShowAndFocusPicker(ProfilePicker::EntryPoint::kProfileMenuManageProfiles,
                     GURL("chrome://profile-picker"));

  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),

      WaitForShow(kProfilePickerViewId),
      InstrumentNonTabWebView(kPickerWebContentsId, web_view()),
      WaitForWebContentsReady(kPickerWebContentsId,
                              GURL("chrome://profile-picker")),
      CheckResult(GetNavState(), Eq(NavState{.entry_count = 1,
                                             .last_committed_entry_index = 0})),

      // Advance to the profile type choice screen.
      EnsurePresent(kPickerWebContentsId, kAddProfileButton),
      MoveMouseTo(kPickerWebContentsId, kAddProfileButton), ClickMouse(),
      WaitForStateChange(
          kPickerWebContentsId,
          UrlEntryMatches(GURL("chrome://profile-picker/new-profile"))),
      CheckResult(GetNavState(), Eq(NavState{.entry_count = 2,
                                             .last_committed_entry_index = 1})),

      // Navigate back with the keyboard.
      SendAccelerator(kProfilePickerViewId, GetAccelerator(IDC_BACK)),
      CheckResult(HasPendingNav(), IsTrue()),
      WaitForStateChange(kPickerWebContentsId,
                         UrlEntryMatches(GURL("chrome://profile-picker"))),
      CheckResult(GetNavState(), Eq(NavState{.entry_count = 2,
                                             .last_committed_entry_index = 0})),

      // Navigating back once again does nothing.
      SendAccelerator(kProfilePickerViewId, GetAccelerator(IDC_BACK)),
      CheckResult(HasPendingNav(), IsFalse()));
}

IN_PROC_BROWSER_TEST_F(ProfilePickerInteractiveUiTest,
                       NavigateBackFromNewProfileWithKeyboard) {
  // Check that when deep-linking into the flow via the "Add profile" menu entry
  // populates the navigation list is populated correctly such that back
  // navigations make sense.
  ShowAndFocusPicker(ProfilePicker::EntryPoint::kProfileMenuAddNewProfile,
                     GURL("chrome://profile-picker/new-profile"));

  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),

      WaitForShow(kProfilePickerViewId),
      InstrumentNonTabWebView(kPickerWebContentsId, web_view()),
      WaitForWebContentsReady(kPickerWebContentsId,
                              GURL("chrome://profile-picker/new-profile")),

      // Even though we start straight on the "new-profile" page, the picker
      // main view should be loaded under it in the nav stack.
      CheckResult(GetNavState(), Eq(NavState{.entry_count = 2,
                                             .last_committed_entry_index = 1})),

      // Focus the window to ensure the keyboard shortcut reaches it.
      Do([&] { SimulateUserActivation(); }),

      // Navigate back with the keyboard.
      SendAccelerator(kProfilePickerViewId, GetAccelerator(IDC_BACK)),
      CheckResult(HasPendingNav(), IsTrue(),
                  /*check_description=*/"HasPendingNav"),
      WaitForStateChange(kPickerWebContentsId,
                         UrlEntryMatches(GURL("chrome://profile-picker"))),
      CheckResult(GetNavState(), Eq(NavState{.entry_count = 2,
                                             .last_committed_entry_index = 0})),

      // Navigating back once again does nothing.
      SendAccelerator(kProfilePickerViewId, GetAccelerator(IDC_BACK)),
      CheckResult(HasPendingNav(), IsFalse())

  );
}
