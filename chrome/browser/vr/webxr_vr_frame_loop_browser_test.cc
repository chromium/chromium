// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/environment.h"
#include "base/run_loop.h"
#include "chrome/browser/vr/test/mock_xr_device_hook_base.h"
#include "chrome/browser/vr/test/multi_class_browser_test.h"
#include "chrome/browser/vr/test/ui_utils.h"
#include "chrome/browser/vr/test/webxr_vr_browser_test.h"

namespace vr {

namespace {

class MyXRMock : public MockXRDeviceHookBase {
 public:
  void OnFrameSubmitted(
      device_test::mojom::SubmittedFrameDataPtr frame_data,
      device_test::mojom::XRTestHook::OnFrameSubmittedCallback callback) final;

  // The test waits for a submitted frame before returning.
  void WaitForFrames(int count) {
    DCHECK(!wait_loop_);
    wait_frame_count_ = num_frames_submitted_ + count;

    base::RunLoop* wait_loop =
        new base::RunLoop(base::RunLoop::Type::kNestableTasksAllowed);
    wait_loop_ = wait_loop;
    wait_loop->Run();
    delete wait_loop;
  }

  int FramesSubmitted() const { return num_frames_submitted_; }

 private:
  // Set to null on background thread after calling Quit(), so we can ensure we
  // only call Quit once.
  base::RunLoop* wait_loop_ = nullptr;

  int wait_frame_count_ = 0;
  int num_frames_submitted_ = 0;
};

void MyXRMock::OnFrameSubmitted(
    device_test::mojom::SubmittedFrameDataPtr frame_data,
    device_test::mojom::XRTestHook::OnFrameSubmittedCallback callback) {
  num_frames_submitted_++;
  if (num_frames_submitted_ >= wait_frame_count_ && wait_frame_count_ > 0 &&
      wait_loop_) {
    wait_loop_->Quit();
    wait_loop_ = nullptr;
  }

  std::move(callback).Run();
}

}  // namespace

WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestNoStalledFrameLoop) {
  MyXRMock my_mock;

  // Load the test page, and enter presentation.
  t->LoadFileAndAwaitInitialization("webxr_no_stalled_frame_loop");
  t->EnterSessionWithUserGestureOrFail();

  // Wait for 2 frames to be submitted back to the device, but the js frame loop
  // should've only been called once.
  my_mock.WaitForFrames(2);
  ASSERT_TRUE(t->RunJavaScriptAndExtractBoolOrFail("frame_count === 1"));

  // Now restart the frame loop and wait for another frame to get submitted.
  t->RunJavaScriptOrFail("setBaseLayer()");
  t->PollJavaScriptBooleanOrFail("frame_count >= 2",
                                 XrBrowserTestBase::kPollTimeoutMedium);

  t->AssertNoJavaScriptErrors();
}

WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestLateSetOfBaseLayer) {
  MyXRMock my_mock;

  // Load the test page, and enter presentation.
  t->LoadFileAndAwaitInitialization("webxr_set_base_layer_late");
  t->EnterSessionWithUserGestureOrFail();

  // Poll and have the javascript yield for 500 ms.  This should give us enough
  // time for any frame requests that were going to propagate to propagate.
  t->RunJavaScriptOrFail("delayMilliseconds(500)");
  t->PollJavaScriptBooleanOrFail("delay_ended");

  // No frames should have been submitted to either the JS or the runtime.
  ASSERT_TRUE(t->RunJavaScriptAndExtractBoolOrFail("frame_count === 0"));
  ASSERT_EQ(my_mock.FramesSubmitted(), 0);

  // Now restart the frame loop and wait for a frame to get submitted.
  t->RunJavaScriptOrFail("setBaseLayer()");
  t->PollJavaScriptBooleanOrFail("frame_count >= 1",
                                 XrBrowserTestBase::kPollTimeoutMedium);

  t->AssertNoJavaScriptErrors();
}

}  // namespace vr
