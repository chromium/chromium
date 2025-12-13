// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/mock_xr_device_hook_base.h"
#include "chrome/browser/vr/test/multi_class_browser_test.h"
#include "chrome/browser/vr/test/ui_utils.h"
#include "chrome/browser/vr/test/webxr_vr_browser_test.h"
#include "device/vr/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_OPENXR)

namespace vr {

namespace {

const float kIpd = 0.7f;

class MyXRMock : public MockXRDeviceHookBase {
 public:
  void WaitGetDeviceConfig(
      device_test::mojom::XRTestHook::WaitGetDeviceConfigCallback callback)
      final {
    DCHECK_CALLED_ON_VALID_SEQUENCE(mock_device_sequence_);
    std::move(callback).Run({.interpupillary_distance = kIpd});
  }

  void WaitGetPresentingPose(
      device_test::mojom::XRTestHook::WaitGetPresentingPoseCallback callback)
      final {
    DCHECK_CALLED_ON_VALID_SEQUENCE(mock_device_sequence_);
    gfx::Transform pose;
    uint32_t frame_count = GetFrameCount();
    // Add a translation with the value of the current frame count.
    pose.Translate3d(frame_count, frame_count, frame_count);
    // Rotate about the Y-axis similarly.
    pose.RotateAboutYAxis(frame_count);

    std::move(callback).Run(std::move(pose));
  }
};

}  // namespace

// Tests secondary views in WebXR. This test requests the 'secondary-views'
// feature when requesting a session and verifies that secondary views are
// exposed. Secondary views are currently only supported in the OpenXR backend.
IN_PROC_BROWSER_TEST_F(WebXrVrOpenXrBrowserTest, TestSecondaryViews) {
  UiUtils::DisableOverlayForTesting();
  MyXRMock mock;

  LoadFileAndAwaitInitialization("test_openxr_secondary_views");
  EnterSessionWithUserGestureOrFail();

  WaitOnJavaScriptStep();
  AssertNoJavaScriptErrors();

  EndTest();
}

}  // namespace vr

#endif  // BUILDFLAG(ENABLE_OPENXR)
