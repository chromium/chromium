// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "ash/ash_element_identifiers.h"
#include "ash/public/cpp/debug_utils.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/style/close_button.h"
#include "ash/wm/desks/desk_action_view.h"
#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desk_textfield.h"
#include "ash/wm/desks/desks_test_api.h"
#include "ash/wm/window_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gtest_tags.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_switches.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/env.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

namespace ash {
namespace {

// Observes the expansion state of the overview desk bar.
class DeskBarExpandedObserver : public ui::test::StateObserver<bool> {
 public:
  explicit DeskBarExpandedObserver(DeskBarViewBase* desk_bar) {
    if (desk_bar->state() == DeskBarViewBase::State::kExpanded) {
      OnStateObserverStateChanged(true);
    } else {
      DesksTestApi::SetDeskBarUiUpdateCallback(
          desk_bar,
          base::BindOnce(&DeskBarExpandedObserver::OnStateObserverStateChanged,
                         weak_ptr_factory_.GetWeakPtr(), true));
    }
  }
  ~DeskBarExpandedObserver() override = default;

 private:
  base::WeakPtrFactory<DeskBarExpandedObserver> weak_ptr_factory_{this};
};

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(DeskBarExpandedObserver,
                                    kDeskBarExpandedState);

class DesksInteractiveUiTest : public InteractiveAshTest {
 public:
  DesksInteractiveUiTest() {
    scoped_feature_list_.InitAndDisableFeature({features::kForestFeature});
  }

  // This function is used to name the DeskMiniView that is associated with the
  // desk at `desk_index`. This will find the right view regardless of how the
  // mini views may be organized as child views.
  auto NameDeskMiniView(std::string_view name, int desk_index) {
    return NameDescendantView(
        kOverviewDeskBarElementId, name,
        base::BindRepeating(
            [](int desk_index, const views::View* view) {
              if (const auto* const mini_view =
                      views::AsViewClass<DeskMiniView>(view)) {
                if (DesksController::Get()->GetDeskIndex(mini_view->desk()) ==
                    desk_index) {
                  return true;
                }
              }
              return false;
            },
            desk_index));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DesksInteractiveUiTest, DesksBasic) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-c873d6a0-8e97-4a94-bb28-c1b9e6af4a51");
  SetupContextWidget();

  // Open two browser windows.
  GURL version_url("chrome://version");
  ASSERT_TRUE(CreateBrowserWindow(version_url));
  GURL blank_url("about:blank");
  ASSERT_TRUE(CreateBrowserWindow(blank_url));

  auto* browser_list = BrowserList::GetInstance();
  ASSERT_EQ(browser_list->size(), 2u);

  aura::Window* browser1_window =
      browser_list->get(0)->window()->GetNativeWindow();
  aura::Window* browser2_window =
      browser_list->get(1)->window()->GetNativeWindow();

  ASSERT_TRUE(browser1_window);
  ASSERT_TRUE(browser2_window);

  // This is not ideal. Animations are currently disabled because the test will
  // otherwise break when reordering desks. This in turn is because the step
  // that interacts with the desk action buttons won't work until an animation
  // has completed. In order to run with animations enabled, we would need a way
  // to observe animations and have the test wait until it has completed.
  ui::ScopedAnimationDurationScaleMode duration_scale(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  constexpr char kMiniView1[] = "desk1-mv";
  constexpr char kMiniView2[] = "desk2-mv";

  constexpr char kMiniView2Edit[] = "desk2-mv-edit";

  constexpr char kMiniView1Preview[] = "desk1-mv-preview";
  constexpr char kMiniView2Preview[] = "desk2-mv-preview";

  constexpr char kActionView[] = "action-view";
  constexpr char kCombineDesksButton[] = "combine-desks";

  // This will eventually hold a pointer to the overview desk bar.
  DeskBarViewBase* desk_bar_view = nullptr;

  RunTestSequence(
      Log("Enter overview mode"), Do([] {
        ui_controls::SendKeyPress(/*window=*/nullptr,
                                  ui::VKEY_MEDIA_LAUNCH_APP1,
                                  /*control=*/false, /*shift=*/false,
                                  /*alt=*/false, /*command=*/false);
      }),

      Log("Wait for the overview desk bar to show"),
      AfterShow(kOverviewDeskBarElementId,
                [&desk_bar_view](ui::TrackedElement* el) {
                  desk_bar_view = AsView<DeskBarViewBase>(el);
                  ASSERT_TRUE(desk_bar_view);
                }),

      ObserveState(kDeskBarExpandedState, std::ref(desk_bar_view)),

      // Press the new desk button.
      PressButton(kOverviewDeskBarNewDeskButtonElementId),
      WaitForState(kDeskBarExpandedState, true),

      // We should now have two mini views.
      CheckView(
          kOverviewDeskBarElementId,
          [](DeskBarViewBase* desk_bar) {
            return desk_bar->mini_views().size();
          },
          testing::Eq(2u)),

      NameDeskMiniView(kMiniView1, /*desk_index=*/0),
      NameDeskMiniView(kMiniView2, /*desk_index=*/1),

      // Verify that the name field of the new desk has focus.
      NameDescendantViewByType<DeskTextfield>(kMiniView2, kMiniView2Edit),
      CheckViewProperty(kMiniView2Edit, &views::View::HasFocus, true),

      // Name the new desk and hit enter.
      EnterText(kMiniView2Edit, u"new desk"),
      SendAccelerator(kMiniView2Edit, ui::Accelerator(ui::VKEY_RETURN, 0)),

      NameDescendantViewByType<DeskPreviewView>(kMiniView1, kMiniView1Preview),
      NameDescendantViewByType<DeskPreviewView>(kMiniView2, kMiniView2Preview),

      // Activate the mini view of the created desk. Then swap the two desks via
      // hotkey.
      Do([] {
        ui_controls::SendKeyPress(/*window=*/nullptr, ui::VKEY_TAB,
                                  /*control=*/false, /*shift=*/true,
                                  /*alt=*/false, /*command=*/false);
        ui_controls::SendKeyPress(/*window=*/nullptr, ui::VKEY_TAB,
                                  /*control=*/false, /*shift=*/true,
                                  /*alt=*/false, /*command=*/false);
        ui_controls::SendKeyPress(/*window=*/nullptr, ui::VKEY_LEFT,
                                  /*control=*/true, /*shift=*/false,
                                  /*alt=*/false, /*command=*/false);
      }),

      // Verify that the desks have been swapped.
      CheckResult(
          [] { return DesksController::Get()->GetDeskAtIndex(0)->name(); },
          testing::Eq(u"new desk")),

      // Select one of the browser windows and move it to another desk.
      Do([] {
        ui_controls::SendKeyPress(/*window=*/nullptr, ui::VKEY_TAB,
                                  /*control=*/false, /*shift=*/true,
                                  /*alt=*/false, /*command=*/false);
        ui_controls::SendKeyPress(/*window=*/nullptr, ui::VKEY_OEM_4,
                                  /*control=*/false, /*shift=*/true,
                                  /*alt=*/false, /*command=*/true);
      }),

      // Verify that the two browsers now live on different desks.
      Check([&] {
        return browser1_window->parent() != browser2_window->parent();
      }),

      // Make sure that the cursor is on the preview for the new desk.
      MoveMouseTo(kMiniView2Preview),
      NameDescendantViewByType<DeskActionView>(kMiniView2, kActionView),

      // This depends on the ordering in which the button views are added to the
      // action view.
      NameDescendantViewByType<CloseButton>(kActionView, kCombineDesksButton,
                                            /*index=*/0),
      CheckViewProperty(kCombineDesksButton, &CloseButton::GetVisible, true),
      // We would like to move the cursor to the location of this button and
      // press it, but for an unknown reason, the size of the button is 0,0 at
      // this time.
      PressButton(kCombineDesksButton),

      // Verify that the two browsers are now on the same desk.
      Check([&] {
        return browser1_window->parent() == browser2_window->parent();
      }),

      Log("Test done"));

  auto* browser_list2 = BrowserList::GetInstance();
  // Copy the browser list to avoid mutating it during iteration.
  std::vector<Browser*> browsers(browser_list2->begin(), browser_list2->end());
  for (Browser* browser : browsers) {
    CloseBrowserSynchronously(browser);
  }
}

}  // namespace
}  // namespace ash
