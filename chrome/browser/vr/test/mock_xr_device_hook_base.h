// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_MOCK_XR_DEVICE_HOOK_BASE_H_
#define CHROME_BROWSER_VR_TEST_MOCK_XR_DEVICE_HOOK_BASE_H_

#include <atomic>
#include <memory>
#include <optional>
#include <queue>
#include <vector>

#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/threading/thread.h"
#include "chrome/browser/vr/test/mock_xr_input_source.h"
#include "device/vr/public/mojom/openxr_interaction_profile_type.mojom.h"
#include "device/vr/public/mojom/test/controller_frame_data.h"
#include "device/vr/public/mojom/test/view_data.h"
#include "device/vr/public/mojom/test/xr_test_hook.test-mojom.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/public/mojom/xr_hand_tracking_data.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/transform.h"

// A Mock XR Device implementing device_test::mojom::XRTestHook. This allows
// the mock OpenXR runtime to synchronously query simulated frame, pose, and
// input data, while allowing tests to inspect submitted frames and inject
// runtime events.
//
// Threading Model:
// - The test runner / test assertions run on the main browser UI thread.
// - Incoming synchronous Mojo queries from the runtime are handled on a
//   dedicated background thread (`MockXRDeviceHookThread`) to avoid deadlocks.
// - Sequence checkers (`mock_device_sequence_` and `main_sequence_`) ensure
//   proper thread boundary separation.
// Please refer to xr_browser_tests.md for complete details.
class MockXRDeviceHookBase : public device_test::mojom::XRTestHook {
 public:
  MockXRDeviceHookBase();
  ~MockXRDeviceHookBase() override;

  // device_test::mojom::XRTestHook
  void OnFrameSubmitted(
      const std::vector<device::ViewData>& views,
      const std::vector<device::LayerData>& layers,
      device_test::mojom::XRTestHook::OnFrameSubmittedCallback callback) final;
  void WaitGetDeviceConfig(
      device_test::mojom::XRTestHook::WaitGetDeviceConfigCallback callback)
      final;
  void WaitGetFrameData(
      device_test::mojom::XRTestHook::WaitGetFrameDataCallback callback) final;
  void WaitGetEventData(
      device_test::mojom::XRTestHook::WaitGetEventDataCallback callback) final;
  void WaitGetCanCreateSession(
      device_test::mojom::XRTestHook::WaitGetCanCreateSessionCallback callback)
      final;
  void WaitGetVisibilityMask(
      uint32_t view_index,
      device_test::mojom::XRTestHook::WaitGetVisibilityMaskCallback callback)
      final;

  // Input source management:
  MockXRInputSource& CreateInputSource(device::mojom::XRHandedness handedness =
                                           device::mojom::XRHandedness::NONE,
                                       bool has_hand_tracking = false);
  MockXRInputSource& CreateMinimalGamepad(
      device::mojom::XRHandedness handedness =
          device::mojom::XRHandedness::RIGHT);

  // Device configuration:
  void SetDeviceConfig(const device::DeviceConfig& config);

  // Head pose:
  void SetHeadPose(const gfx::Transform& pose);

  // Event simulation:
  void SimulateSessionLost();
  void SimulateVisibilityBlurred();
  void SimulateInteractionProfileChanged(
      device::mojom::OpenXrInteractionProfileType profile);
  void SimulateInstanceLost();

  // MockXRDeviceHookBase
  void PopulateEvent(device_test::mojom::EventData data);
  void StopHooking();
  void SetCanCreateSession(bool can_create_session);
  void SetVisibilityMaskForTesting(uint32_t view_index,
                                   device::mojom::XRVisibilityMaskPtr mask);
  uint32_t GetFrameCount() { return frame_count_; }
  void WaitNumFrames(uint32_t num_frames);
  void WaitForTotalFrameCount(uint32_t total_count);

 protected:
  // This allows subclasses to update frame data (e.g. SetHeadPose) before
  // the frame data is returned to the runtime.
  virtual void UpdateFrameDataUnlocked() {}

  // This allows subclasses to process the submitted frame. This method will be
  // called *after* the frame count has been incremented but *before* any
  // potential wait loop is signaled.
  virtual void ProcessSubmittedFrameUnlocked(
      const std::vector<device::ViewData>& views,
      const std::vector<device::LayerData>& layers) {}

  SEQUENCE_CHECKER(mock_device_sequence_);
  SEQUENCE_CHECKER(main_sequence_);
  base::Lock lock_;
  std::unique_ptr<base::Thread> thread_;

  std::queue<device_test::mojom::EventData> event_data_queue_ GUARDED_BY(lock_);
  absl::flat_hash_map<uint32_t, device::mojom::XRVisibilityMaskPtr>
      visibility_masks_ GUARDED_BY(lock_);
  std::optional<gfx::Transform> head_pose_ GUARDED_BY(lock_);
  device::DeviceConfig config_ GUARDED_BY(lock_) = {
      .interpupillary_distance = 0.1f};

 private:
  mojo::Receiver<device_test::mojom::XRTestHook> receiver_{this};
  std::atomic_bool can_create_session_ = true;
  std::atomic_uint32_t frame_count_ = 0;
  std::atomic_uint32_t target_frame_count_ = 0;
  std::vector<std::unique_ptr<MockXRInputSource>> input_sources_
      GUARDED_BY(lock_);

  base::RepeatingClosure wait_loop_quit_closure_ GUARDED_BY(lock_);
};

#endif  // CHROME_BROWSER_VR_TEST_MOCK_XR_DEVICE_HOOK_BASE_H_
