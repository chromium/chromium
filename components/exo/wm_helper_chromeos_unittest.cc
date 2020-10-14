// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wm_helper_chromeos.h"
#include "ash/frame_throttler/frame_throttling_controller.h"
#include "ash/shell.h"
#include "components/exo/mock_vsync_timing_observer.h"
#include "components/exo/test/exo_test_base.h"

namespace exo {

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
      base::TimeDelta::FromSeconds(1) / ftc->throttled_fps();
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
}  // namespace exo
