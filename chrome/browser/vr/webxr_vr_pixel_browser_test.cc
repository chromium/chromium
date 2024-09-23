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


namespace vr {

class MyXRMock : public MockXRDeviceHookBase {
 public:
  void OnFrameSubmitted(
      std::vector<device_test::mojom::ViewDataPtr> views,
      device_test::mojom::XRTestHook::OnFrameSubmittedCallback callback) final;

  void WaitForFrame() {
    DCHECK(!wait_loop_);
    if (num_submitted_frames_ > 0)
      return;

    wait_loop_ = std::make_unique<base::RunLoop>(
        base::RunLoop::Type::kNestableTasksAllowed);
    can_signal_wait_loop_ = true;

    wait_loop_->Run();

    can_signal_wait_loop_ = false;
    wait_loop_ = nullptr;
  }

  device_test::mojom::ColorPtr last_submitted_color_ = {};
  unsigned int num_submitted_frames_ = 0;

 private:
  std::unique_ptr<base::RunLoop> wait_loop_ = nullptr;

  // Used to track both if `wait_loop_` is valid in a thread-safe manner or if
  // it has already had quit signaled on it, since `AnyQuitCalled` won't update
  // until the `Quit` task has posted to the main thread.
  std::atomic_bool can_signal_wait_loop_ = false;
};

void MyXRMock::OnFrameSubmitted(
    std::vector<device_test::mojom::ViewDataPtr> views,
    device_test::mojom::XRTestHook::OnFrameSubmittedCallback callback) {
  // Since we clear the entire context to a single color (see onXRFrame() in
  // webxr_boilerplate.js), every view in the frame has the same color.
  last_submitted_color_ = std::move(views[0]->color);
  num_submitted_frames_++;

  if (can_signal_wait_loop_) {
    wait_loop_->Quit();
    can_signal_wait_loop_ = false;
  }

  std::move(callback).Run();
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

  my_mock.WaitForFrame();

  auto expected = device_test::mojom::Color::New(0, 0, 255, 255);
  EXPECT_EQ(expected->r, my_mock.last_submitted_color_->r)
      << "Red channel of submitted color does not match expectation";
  EXPECT_EQ(expected->g, my_mock.last_submitted_color_->g)
      << "Green channel of submitted color does not match expectation";
  EXPECT_EQ(expected->b, my_mock.last_submitted_color_->b)
      << "Blue channel of submitted color does not match expectation";
  EXPECT_EQ(expected->a, my_mock.last_submitted_color_->a)
      << "Alpha channel of submitted color does not match expectation";
}

WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestPresentationPixels) {
  TestPresentationPixelsImpl(t, "test_webxr_pixels");
}

}  // namespace vr
