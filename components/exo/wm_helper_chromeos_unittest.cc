// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wm_helper_chromeos.h"

#include <memory>

#include "ash/frame_throttler/frame_throttling_controller.h"
#include "ash/shell.h"
#include "base/callback_helpers.h"
#include "base/notreached.h"
#include "components/exo/mock_vsync_timing_observer.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/wm_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/gfx/geometry/point_f.h"

namespace exo {
namespace {

using ::ui::mojom::DragOperation;

class MockDragDropObserver : public WMHelper::DragDropObserver {
 public:
  MockDragDropObserver(DragOperation drop_result) : drop_result_(drop_result) {}
  ~MockDragDropObserver() override = default;

  // WMHelper::DragDropObserver:
  void OnDragEntered(const ui::DropTargetEvent& event) override {}
  aura::client::DragUpdateInfo OnDragUpdated(
      const ui::DropTargetEvent& event) override {
    return aura::client::DragUpdateInfo();
  }
  void OnDragExited() override {}
  DragOperation OnPerformDrop(const ui::DropTargetEvent& event) override {
    return drop_result_;
  }
  WMHelper::DropCallback GetDropCallback(
      const ui::DropTargetEvent& event) override {
    NOTIMPLEMENTED();
    return base::NullCallback();
  }

 private:
  DragOperation drop_result_;
};

}  // namespace

using WMHelperChromeOSTest = test::ExoTestBase;

TEST_F(WMHelperChromeOSTest, FrameThrottling) {
  WMHelperChromeOS* wm_helper_chromeos =
      static_cast<WMHelperChromeOS*>(wm_helper());
  wm_helper_chromeos->AddFrameThrottlingObserver();
  VSyncTimingManager& vsync_timing_manager =
      wm_helper_chromeos->GetVSyncTimingManager();
  MockVSyncTimingObserver observer;
  vsync_timing_manager.AddObserver(&observer);
  ash::FrameThrottlingController* ftc =
      ash::Shell::Get()->frame_throttling_controller();

  // Throttling should be off by default.
  EXPECT_EQ(vsync_timing_manager.throttled_interval(), base::TimeDelta());

  // Create two arc windows.
  std::unique_ptr<aura::Window> arc_window_1 =
      CreateAppWindow(gfx::Rect(), ash::AppType::ARC_APP);
  std::unique_ptr<aura::Window> arc_window_2 =
      CreateAppWindow(gfx::Rect(), ash::AppType::ARC_APP);

  // Starting throttling on one of the two arc windows will have no effect on
  // vsync time.
  EXPECT_CALL(observer, OnUpdateVSyncParameters(testing::_, testing::_))
      .Times(0);
  ftc->StartThrottling({arc_window_1.get()});
  EXPECT_EQ(vsync_timing_manager.throttled_interval(), base::TimeDelta());

  // Both windows are to be throttled, vsync timing will be adjusted.
  base::TimeDelta throttled_interval =
      base::TimeDelta::FromHz(ftc->throttled_fps());
  EXPECT_CALL(observer,
              OnUpdateVSyncParameters(testing::_, throttled_interval));
  ftc->StartThrottling({arc_window_1.get(), arc_window_2.get()});
  EXPECT_EQ(vsync_timing_manager.throttled_interval(), throttled_interval);

  EXPECT_CALL(observer,
              OnUpdateVSyncParameters(testing::_,
                                      viz::BeginFrameArgs::DefaultInterval()));
  ftc->EndThrottling();
  EXPECT_EQ(vsync_timing_manager.throttled_interval(), base::TimeDelta());

  vsync_timing_manager.RemoveObserver(&observer);
  wm_helper_chromeos->RemoveFrameThrottlingObserver();
}

TEST_F(WMHelperChromeOSTest, MultipleDragDropObservers) {
  WMHelperChromeOS* wm_helper_chromeos =
      static_cast<WMHelperChromeOS*>(wm_helper());
  MockDragDropObserver observer_no_drop(DragOperation::kNone);
  MockDragDropObserver observer_copy_drop(DragOperation::kCopy);

  wm_helper_chromeos->AddDragDropObserver(&observer_no_drop);

  ui::DropTargetEvent target_event(ui::OSExchangeData(), gfx::PointF(),
                                   gfx::PointF(), ui::DragDropTypes::DRAG_NONE);
  DragOperation op = wm_helper_chromeos->OnPerformDrop(
      target_event, std::make_unique<ui::OSExchangeData>());
  EXPECT_EQ(op, DragOperation::kNone);

  wm_helper_chromeos->AddDragDropObserver(&observer_copy_drop);
  op = wm_helper_chromeos->OnPerformDrop(
      target_event, std::make_unique<ui::OSExchangeData>());
  EXPECT_NE(op, DragOperation::kNone);

  wm_helper_chromeos->RemoveDragDropObserver(&observer_no_drop);
  wm_helper_chromeos->RemoveDragDropObserver(&observer_copy_drop);
}

}  // namespace exo
