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
  std::vector<device::ViewData> views;
  std::optional<gfx::Transform> pose;
  device::DeviceConfig config;
};

class MyXRMock : public MockXRDeviceHookBase {
 public:
  void ProcessSubmittedFrameUnlocked(
      const std::vector<device::ViewData>& views) final;
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

  base::Lock frame_data_lock;
  std::vector<Frame> submitted_frames GUARDED_BY(frame_data_lock);

  device::DeviceConfig GetDeviceConfig() {
    // Stateless helper function may be called on any thread.
    return {.interpupillary_distance = kIPD};
  }

 private:
  std::optional<gfx::Transform> last_immersive_frame_data
      GUARDED_BY(frame_data_lock);
  std::atomic_int frame_id_ = 0;
};

uint32_t ParseColorFrameId(const device::Color& color) {
  // Corresponding math in test_webxr_poses.html.
  uint32_t frame_id =
      static_cast<uint32_t>(color.r) + 256 * color.g + 256 * 256 * color.b;
  return frame_id;
}

void MyXRMock::ProcessSubmittedFrameUnlocked(
    const std::vector<device::ViewData>& views) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mock_device_sequence_);
  base::AutoLock lock(frame_data_lock);
  // Since we clear the entire context to a single color, every view in the
  // frame has the same color (see onImmersiveXRFrameCallback in
  // test_webxr_poses.html).
  uint32_t frame_id = ParseColorFrameId(views[0].color);
  DVLOG(3) << "Frame Submitted: " << GetFrameCount() << " " << frame_id;
  submitted_frames.push_back(
      {views, last_immersive_frame_data, GetDeviceConfig()});

  ASSERT_TRUE(last_immersive_frame_data)
      << "Frame submitted without any frame data provided";

  // We expect a waitGetPoses, then 2 submits (one for each eye), so after 2
  // submitted frames don't use the same frame_data again.
  if (GetFrameCount() % 2 == 0) {
    last_immersive_frame_data = std::nullopt;
  }
}

void MyXRMock::WaitGetMagicWindowPose(
    device_test::mojom::XRTestHook::WaitGetMagicWindowPoseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mock_device_sequence_);
  // Almost identity matrix - enough different that we can identify if magic
  // window poses are used instead of presenting poses.
  gfx::Transform pose;
  pose.set_rc(1, 1, -1);
  std::move(callback).Run(std::move(pose));
}

void MyXRMock::WaitGetPresentingPose(
    device_test::mojom::XRTestHook::WaitGetPresentingPoseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mock_device_sequence_);
  DLOG(ERROR) << "WaitGetPresentingPose: " << frame_id_;

  gfx::Transform pose;

  // Add a translation so each frame gets a different transform, and so its easy
  // to identify what the expected pose is.
  pose.Translate3d(0, 0, frame_id_);

  frame_id_++;
  {
    base::AutoLock lock(frame_data_lock);
    last_immersive_frame_data = pose;
  }

  std::move(callback).Run(pose);
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
  return GetMatrixAsString(*(frame.pose));
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
  my_mock.WaitForTotalFrameCount(20);

  // Exit presentation.
  t->EndSessionOrFail();

  // Stop hooking the VR runtime so we can safely analyze our cached data
  // without incoming calls (there may be leftover mojo messages queued).
  my_mock.StopHooking();

  // While finishing up the test doesn't need the lock, the fact that we've
  // disconnected the mock device means that it's safe to just hold onto it
  // until the end of the function.
  base::AutoLock lock(my_mock.frame_data_lock);

  // Analyze the submitted frames - check for a few things:
  // 1. Each frame id should be submitted at most once for each of the left and
  // right eyes.
  // 2. The pose that WebXR used for rendering the submitted frame should be the
  // one that we expected.
  std::set<uint32_t> seen_left;
  std::set<uint32_t> seen_right;
  uint32_t max_frame_id = 0;
  for (const auto& frame : my_mock.submitted_frames) {
    for (const auto& data : frame.views) {
      // The test page encodes the frame id as the clear color.
      uint32_t frame_id = ParseColorFrameId(data.color);

      // Validate that each frame is only seen once for each eye.
      DLOG(ERROR) << "Frame id: " << frame_id;
      if (data.eye == device::XrEye::kLeft) {
        ASSERT_TRUE(seen_left.find(frame_id) == seen_left.end())
            << "Frame for left eye submitted more than once";
        seen_left.insert(frame_id);
      } else if (data.eye == device::XrEye::kRight) {
        ASSERT_TRUE(seen_right.find(frame_id) == seen_right.end())
            << "Frame for right eye submitted more than once";
        seen_right.insert(frame_id);
      } else {
        NOTREACHED();
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
