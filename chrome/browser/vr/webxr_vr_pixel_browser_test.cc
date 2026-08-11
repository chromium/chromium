// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atomic>
#include <memory>

#include "base/environment.h"
#include "base/files/file.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/vr/test/mock_xr_device_hook_base.h"
#include "chrome/browser/vr/test/multi_class_browser_test.h"
#include "chrome/browser/vr/test/ui_utils.h"
#include "chrome/browser/vr/test/webxr_vr_browser_test.h"
#include "third_party/skia/include/core/SkColor.h"

namespace vr {

class MyXRMock : public MockXRDeviceHookBase {
 public:
  void ProcessSubmittedFrameUnlocked(
      const std::vector<device::ViewData>& views,
      const std::vector<device::LayerData>& layers) final;

  base::Lock color_lock;
  SkColor last_submitted_color_ GUARDED_BY(color_lock);
};

void MyXRMock::ProcessSubmittedFrameUnlocked(
    const std::vector<device::ViewData>& views,
    const std::vector<device::LayerData>& layers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mock_device_sequence_);
  base::AutoLock lock(color_lock);
  // Since we clear the entire context to a single color (see onXRFrame() in
  // webxr_boilerplate.js), every view in the frame has the same color.
  last_submitted_color_ = views[0].color;
}

// Pixel test for WebXR - start presentation, submit frames, get data back
// out. Validates that a pixel was rendered with the expected color.
void TestPresentationPixelsImpl(WebXrVrBrowserTestBase* t,
                                std::string filename) {
  // Disable frame-timeout UI to test what WebXR renders.
  UiUtils::DisableOverlayForTesting();
  MyXRMock my_mock;

  // Load the test page, and enter presentation.
  t->LoadFileAndAwaitInitialization(filename);
  t->EnterSessionWithUserGestureOrFail();

  // Wait for JavaScript to submit at least one frame.
  ASSERT_TRUE(
      t->PollJavaScriptBoolean("hasPresentedFrame", t->kPollTimeoutMedium))
      << "No frame submitted";

  // Tell JavaScript that it is done with the test.
  t->ExecuteStepAndWait("finishTest()");
  t->EndTest();

  my_mock.WaitForTotalFrameCount(1);

  base::AutoLock lock(my_mock.color_lock);
  EXPECT_EQ(SK_ColorBLUE, my_mock.last_submitted_color_);
}

WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestPresentationPixels) {
  TestPresentationPixelsImpl(t, "test_webxr_pixels");
}

}  // namespace vr
