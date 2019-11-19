// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/environment.h"
#include "base/files/file.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/vr/test/mock_xr_device_hook_base.h"
#include "chrome/browser/vr/test/multi_class_browser_test.h"
#include "chrome/browser/vr/test/ui_utils.h"
#include "chrome/browser/vr/test/webxr_vr_browser_test.h"

#include <memory>

namespace vr {

class MyXRMock : public MockXRDeviceHookBase {
 public:
  void OnFrameSubmitted(
      device_test::mojom::SubmittedFrameDataPtr frame_data,
      device_test::mojom::XRTestHook::OnFrameSubmittedCallback callback) final;

  void WaitForFrame() {
    DCHECK(!wait_loop_);
    if (num_submitted_frames_ > 0)
      return;

    wait_loop_ = new base::RunLoop(base::RunLoop::Type::kNestableTasksAllowed);
    wait_loop_->Run();
    delete wait_loop_;
    wait_loop_ = nullptr;
  }

  device_test::mojom::ColorPtr last_submitted_color_ = {};
  unsigned int num_submitted_frames_ = 0;

 private:
  base::RunLoop* wait_loop_ = nullptr;
};

void MyXRMock::OnFrameSubmitted(
    device_test::mojom::SubmittedFrameDataPtr frame_data,
    device_test::mojom::XRTestHook::OnFrameSubmittedCallback callback) {
  last_submitted_color_ = std::move(frame_data->color);
  num_submitted_frames_++;

  if (wait_loop_) {
    wait_loop_->Quit();
  }

  std::move(callback).Run();
}

// Pixel test for WebXR - start presentation, submit frames, get data back
// out. Validates that a pixel was rendered with the expected color.
void TestPresentationPixelsImpl(WebXrVrBrowserTestBase* t,
                                std::string filename) {
  // Disable frame-timeout UI to test what WebXR renders.
  UiUtils::DisableFrameTimeoutForTesting();
  MyXRMock my_mock;

  // Load the test page, and enter presentation.
  t->LoadUrlAndAwaitInitialization(t->GetFileUrlForHtmlTestFile(filename));
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

// TODO(crbug.com/986621) - OpenXR currently hard codes data
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestPresentationPixels) {
  TestPresentationPixelsImpl(t, "test_webxr_pixels");
}

}  // namespace vr
