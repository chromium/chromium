// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wm_helper.h"

#include <memory>
#include <vector>

#include "ash/frame_throttler/frame_throttling_controller.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "components/exo/mock_vsync_timing_observer.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/wm_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/gfx/geometry/point_f.h"

namespace exo {

using ::ui::mojom::DragOperation;
using WMHelperTest = test::ExoTestBase;

TEST_F(WMHelperTest, FrameThrottling) {
  WMHelper* wm_helper_chromeos = static_cast<WMHelper*>(wm_helper());
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
      CreateAppWindow(gfx::Rect(), chromeos::AppType::ARC_APP);
  std::unique_ptr<aura::Window> arc_window_2 =
      CreateAppWindow(gfx::Rect(), chromeos::AppType::ARC_APP);

  // Starting throttling on one of the two arc windows will have no effect on
  // vsync time.
  EXPECT_CALL(observer, OnUpdateVSyncParameters(testing::_, testing::_))
      .Times(0);
  ftc->StartThrottling({arc_window_1.get()});
  EXPECT_EQ(vsync_timing_manager.throttled_interval(), base::TimeDelta());

  // Both windows are to be throttled, vsync timing will be adjusted.
  base::TimeDelta throttled_interval = ftc->current_throttled_frame_interval();
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

TEST_F(WMHelperTest, DockedModeShouldUseInternalAsDefault) {
  UpdateDisplay("1920x1080*2, 600x400");
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();
  auto display_list = display::Screen::GetScreen()->GetAllDisplays();
  auto first_info = display_manager()->GetDisplayInfo(display_list[0].id());
  auto second_info = display_manager()->GetDisplayInfo(display_list[1].id());
  ASSERT_EQ(gfx::Size(1920, 1080), first_info.size_in_pixel());
  ASSERT_EQ(first_info.id(), display::Display::InternalDisplayId());

  std::vector<display::ManagedDisplayInfo> display_info_list{second_info};
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  ASSERT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().id(),
            second_info.id());

  EXPECT_EQ(2.0f, GetDefaultDeviceScaleFactor());
}

}  // namespace exo
