// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/multi_contents_view_drop_target_controller.h"

#include <memory>

#include "base/check_deref.h"
#include "base/functional/callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_drop_target_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_view_delegate.h"
#include "chrome/browser/ui/views/tabs/dragging/drag_session_data.h"
#include "chrome/browser/ui/views/test/split_view_browser_test_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget_utils.h"

class MockTabDragController : public TabDragDelegate::DragController {
 public:
  MOCK_METHOD(std::unique_ptr<tabs::TabModel>,
              DetachTabAtForInsertion,
              (int drag_idx),
              (override));
  MOCK_METHOD(const DragSessionData&, GetSessionData, (), (const, override));
};

class MultiContentsViewDropTargetControllerBrowserTest
    : public SplitViewBrowserTestMixin<InProcessBrowserTest> {
 protected:
  void SetUpOnMainThread() override {
    SplitViewBrowserTestMixin::SetUpOnMainThread();
    delegate_ = std::make_unique<MultiContentsViewDelegateImpl>(*browser());
    controller_ = std::make_unique<MultiContentsViewDropTargetController>(
        *drop_target_view(), *delegate_.get(),
        g_browser_process->local_state());
  }

  void TearDownOnMainThread() override {
    controller_.reset();
    delegate_.reset();
  }

  const std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      override {
    return {{features::kSideBySide,
             {{features::kSideBySideDropTargetHideForOSWidth.name, "50"}}}};
  }

  MultiContentsViewDropTargetController& controller() { return *controller_; }
  TabStrip* tabstrip() { return browser()->GetBrowserView().tabstrip(); }

  int GetViewWidth() { return browser()->GetBrowserView().width(); }

  void SimulateTabDrag(
      bool is_maximized,
      base::OnceCallback<gfx::Point(int view_width)> get_point_in_view) {
    MockTabDragController mock_tab_drag_controller;
    DragSessionData session_data;
    Tab* tab = tabstrip()->tab_at(0);

    // Construct drag data
    session_data.tab_drag_data_ = {
        TabDragData(tabstrip()->GetDragContext(), tab),
    };
    session_data.tab_drag_data_[0].attached_view = tab;
    EXPECT_CALL(mock_tab_drag_controller, GetSessionData)
        .WillRepeatedly(testing::ReturnRef(session_data));

    // Maximize the browser if necessary
    if (is_maximized) {
      browser()->GetBrowserView().Maximize();
      EXPECT_TRUE(ui_test_utils::WaitForMaximized(browser()));
    }

    const gfx::Point point_in_view =
        std::move(get_point_in_view).Run(GetViewWidth());
    const gfx::Point point_in_screen = views::View::ConvertPointToScreen(
        drop_target_view()->parent(), point_in_view);
    controller().OnTabDragUpdated(mock_tab_drag_controller, point_in_screen);
  }

  // Return whether the drop timer is running which indicates if the drop
  // target will show.
  bool IsDropTimerRunning() {
    return controller().IsDropTimerRunningForTesting();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<MultiContentsViewDropTargetController> controller_;
  std::unique_ptr<MultiContentsViewDelegateImpl> delegate_;
};

IN_PROC_BROWSER_TEST_F(MultiContentsViewDropTargetControllerBrowserTest,
                       OnTabDragUpdatedNotMaximizedWithStartPoint) {
  SimulateTabDrag(false, base::BindOnce([](int view_width) {
                    return gfx::Point(30, 250);
                  }));
  EXPECT_TRUE(IsDropTimerRunning());
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewDropTargetControllerBrowserTest,
                       OnTabDragUpdatedMaximizedWithMiddlePoint) {
  SimulateTabDrag(true, base::BindOnce([](int view_width) {
                    return gfx::Point(view_width / 2, 250);
                  }));
  EXPECT_FALSE(IsDropTimerRunning());
}

// On Linux  and ChromeOS there are test-only discreptencies between the screen
// width and the maximized browser width, so these test need to be skipped.
#if !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(MultiContentsViewDropTargetControllerBrowserTest,
                       OnTabDragUpdatedMaximizedWithStartPoint) {
  SimulateTabDrag(
      true, base::BindOnce([](int view_width) { return gfx::Point(30, 250); }));
  EXPECT_FALSE(IsDropTimerRunning());
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewDropTargetControllerBrowserTest,
                       OnTabDragUpdatedMaximizedWithEndPoint) {
  SimulateTabDrag(true, base::BindOnce([](int view_width) {
                    return gfx::Point(view_width - 10, 250);
                  }));
  EXPECT_FALSE(IsDropTimerRunning());
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewDropTargetControllerBrowserTest,
                       OnTabDragUpdatedNotMaximizedWithEndPoint) {
  SimulateTabDrag(false, base::BindOnce([](int view_width) {
                    return gfx::Point(view_width - 10, 250);
                  }));
  EXPECT_TRUE(IsDropTimerRunning());
}
#endif
