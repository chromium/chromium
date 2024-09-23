// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atomic>
#include <memory>

#include "base/environment.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/vr/test/mock_xr_device_hook_base.h"
#include "chrome/browser/vr/test/multi_class_browser_test.h"
#include "chrome/browser/vr/test/ui_utils.h"
#include "chrome/browser/vr/test/webxr_vr_browser_test.h"

namespace vr {

namespace {

const float kIPD = 0.2f;

struct Frame {
  std::vector<device_test::mojom::ViewDataPtr> views;
  device_test::mojom::PoseFrameDataPtr pose;
  device_test::mojom::DeviceConfigPtr config;
};

class MyXRMock : public MockXRDeviceHookBase {
 public:
  void OnFrameSubmitted(
      std::vector<device_test::mojom::ViewDataPtr> views,
      device_test::mojom::XRTestHook::OnFrameSubmittedCallback callback) final;
  void WaitGetDeviceConfig(
      device_test::mojom::XRTestHook::WaitGetDeviceConfigCallback callback)
      final {
    std::move(callback).Run(GetDeviceConfig());
  }
  void WaitGetPresentingPose(
      device_test::mojom::XRTestHook::WaitGetPresentingPoseCallback callback)
      final;
  void WaitGetMagicWindowPose(
      device_test::mojom::XRTestHook::WaitGetMagicWindowPoseCallback callback)
      final;

  // The test waits for a submitted frame before returning.
  void WaitForFrames(int count) {
    DCHECK(!wait_loop_);
    wait_frame_count_ = count;

    wait_loop_ = std::make_unique<base::RunLoop>(
        base::RunLoop::Type::kNestableTasksAllowed);
    can_signal_wait_loop_ = true;

    wait_loop_->Run();

    can_signal_wait_loop_ = false;
    wait_loop_ = nullptr;
  }

  std::vector<Frame> submitted_frames;
  device_test::mojom::PoseFrameDataPtr last_immersive_frame_data;

  device_test::mojom::DeviceConfigPtr GetDeviceConfig() {
    auto config = device_test::mojom::DeviceConfig::New();
    config->interpupillary_distance = kIPD;
    config->projection_left =
        device_test::mojom::ProjectionRaw::New(0.1f, 0.2f, 0.3f, 0.4f);
    config->projection_right =
        device_test::mojom::ProjectionRaw::New(0.5f, 0.6f, 0.7f, 0.8f);
    return config;
  }

 private:
  std::unique_ptr<base::RunLoop> wait_loop_ = nullptr;

  // Used to track both if `wait_loop_` is valid in a thread-safe manner or if
  // it has already had quit signaled on it, since `AnyQuitCalled` won't update
  // until the `Quit` task has posted to the main thread.
  std::atomic_bool can_signal_wait_loop_ = false;

  int wait_frame_count_ = 0;
  int num_frames_submitted_ = 0;

  int frame_id_ = 0;
};

unsigned int ParseColorFrameId(const device_test::mojom::ColorPtr& color) {
  // Corresponding math in test_webxr_poses.html.
  unsigned int frame_id = static_cast<unsigned int>(color->r) + 256 * color->g +
                          256 * 256 * color->b;
  return frame_id;
}

void MyXRMock::OnFrameSubmitted(
    std::vector<device_test::mojom::ViewDataPtr> views,
    device_test::mojom::XRTestHook::OnFrameSubmittedCallback callback) {
  // Since we clear the entire context to a single color, every view in the
  // frame has the same color (see onImmersiveXRFrameCallback in
  // test_webxr_poses.html).
  unsigned int frame_id = ParseColorFrameId(views[0]->color);
  DLOG(ERROR) << "Frame Submitted: " << num_frames_submitted_ << " "
              << frame_id;
  submitted_frames.push_back(
      {std::move(views), last_immersive_frame_data.Clone(), GetDeviceConfig()});

  num_frames_submitted_++;
  if (num_frames_submitted_ >= wait_frame_count_ && wait_frame_count_ > 0 &&
      can_signal_wait_loop_) {
    wait_loop_->Quit();
    can_signal_wait_loop_ = false;
  }

  ASSERT_TRUE(last_immersive_frame_data)
      << "Frame submitted without any frame data provided";

  // We expect a waitGetPoses, then 2 submits (one for each eye), so after 2
  // submitted frames don't use the same frame_data again.
  if (num_frames_submitted_ % 2 == 0)
    last_immersive_frame_data = nullptr;

  std::move(callback).Run();
}

void MyXRMock::WaitGetMagicWindowPose(
    device_test::mojom::XRTestHook::WaitGetMagicWindowPoseCallback callback) {
  auto pose = device_test::mojom::PoseFrameData::New();

  // Almost identity matrix - enough different that we can identify if magic
  // window poses are used instead of presenting poses.
  pose->device_to_origin =
      gfx::Transform::RowMajor(1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
  std::move(callback).Run(std::move(pose));
}

void MyXRMock::WaitGetPresentingPose(
    device_test::mojom::XRTestHook::WaitGetPresentingPoseCallback callback) {
  DLOG(ERROR) << "WaitGetPresentingPose: " << frame_id_;

  auto pose = device_test::mojom::PoseFrameData::New();

  // Start with identity matrix.
  pose->device_to_origin = gfx::Transform();

  // Add a translation so each frame gets a different transform, and so its easy
  // to identify what the expected pose is.
  pose->device_to_origin->Translate3d(0, 0, frame_id_);

  frame_id_++;
  last_immersive_frame_data = pose.Clone();

  std::move(callback).Run(std::move(pose));
}

std::string GetMatrixAsString(const gfx::Transform& m) {
  // Dump the transpose of the matrix due to device vs. webxr matrix format
  // differences.
  return base::StringPrintf(
      "[%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f]",
      m.rc(0, 0), m.rc(1, 0), m.rc(2, 0), m.rc(3, 0), m.rc(0, 1), m.rc(1, 1),
      m.rc(2, 1), m.rc(3, 1), m.rc(0, 2), m.rc(1, 2), m.rc(2, 2), m.rc(3, 2),
      m.rc(0, 3), m.rc(1, 3), m.rc(2, 3), m.rc(3, 3));
}

std::string GetPoseAsString(const Frame& frame) {
  return GetMatrixAsString(*(frame.pose->device_to_origin));
}

}  // namespace

// Pixel test for WebXR - start presentation, submit frames, get data back out.
// Validates that submitted frames used expected pose.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestPresentationPoses) {
  // Disable frame-timeout UI to test what WebXR renders.
  UiUtils::DisableOverlayForTesting();
  MyXRMock my_mock;

  // Load the test page, and enter presentation.
  t->LoadFileAndAwaitInitialization("test_webxr_poses");
  ASSERT_TRUE(
      t->RunJavaScriptAndExtractBoolOrFail("checkMagicWindowViewOffset()"))
      << "view under Magic Window should not have any offset from frame";
  t->EnterSessionWithUserGestureOrFail();

  // Wait for JavaScript to submit at least one frame.
  ASSERT_TRUE(
      t->PollJavaScriptBoolean("hasPresentedFrame", t->kPollTimeoutShort))
      << "No frame submitted";

  // Render at least 20 frames.  Make sure each has the right submitted pose.
  my_mock.WaitForFrames(20);

  // Exit presentation.
  t->EndSessionOrFail();

  // Stop hooking the VR runtime so we can safely analyze our cached data
  // without incoming calls (there may be leftover mojo messages queued).
  my_mock.StopHooking();

  // Analyze the submitted frames - check for a few things:
  // 1. Each frame id should be submitted at most once for each of the left and
  // right eyes.
  // 2. The pose that WebXR used for rendering the submitted frame should be the
  // one that we expected.
  std::set<unsigned int> seen_left;
  std::set<unsigned int> seen_right;
  unsigned int max_frame_id = 0;
  for (const auto& frame : my_mock.submitted_frames) {
    for (const auto& data : frame.views) {
      // The test page encodes the frame id as the clear color.
      unsigned int frame_id = ParseColorFrameId(data->color);

      // Validate that each frame is only seen once for each eye.
      DLOG(ERROR) << "Frame id: " << frame_id;
      if (data->eye == device_test::mojom::Eye::LEFT) {
        ASSERT_TRUE(seen_left.find(frame_id) == seen_left.end())
            << "Frame for left eye submitted more than once";
        seen_left.insert(frame_id);
      } else if (data->eye == device_test::mojom::Eye::RIGHT) {
        ASSERT_TRUE(seen_right.find(frame_id) == seen_right.end())
            << "Frame for right eye submitted more than once";
        seen_right.insert(frame_id);
      } else {
        NOTREACHED_IN_MIGRATION();
      }

      // Validate that frames arrive in order.
      ASSERT_TRUE(frame_id >= max_frame_id) << "Frame received out of order";
      max_frame_id = frame_id;

      // Validate that the JavaScript-side cache of frames contains our
      // submitted frame.
      ASSERT_TRUE(t->RunJavaScriptAndExtractBoolOrFail(
          base::StringPrintf("checkFrameOccurred(%d)", frame_id)))
          << "JavaScript-side frame cache does not contain submitted frame";

      // Validate that the JavaScript-side cache of frames has the correct pose.
      ASSERT_TRUE(t->RunJavaScriptAndExtractBoolOrFail(base::StringPrintf(
          "checkFramePose(%d, %s)", frame_id, GetPoseAsString(frame).c_str())))
          << "JavaScript-side frame cache has incorrect pose";

      ASSERT_TRUE(t->RunJavaScriptAndExtractBoolOrFail(base::StringPrintf(
          "checkFrameLeftEyeIPD(%d, %f)", frame_id, kIPD / 2)))
          << "JavaScript-side frame cache has incorrect eye position";
    }
  }

  // Tell JavaScript that it is done with the test.
  t->ExecuteStepAndWait("finishTest()");
  t->EndTest();
}

}  // namespace vr
