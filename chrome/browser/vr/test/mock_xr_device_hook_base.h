// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_MOCK_XR_DEVICE_HOOK_BASE_H_
#define CHROME_BROWSER_VR_TEST_MOCK_XR_DEVICE_HOOK_BASE_H_

#include <array>
#include <atomic>
#include <memory>
#include <queue>

#include "base/containers/flat_map.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "device/vr/public/mojom/browser_test_interfaces.mojom-forward.h"
#include "device/vr/test/test_hook.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

// A Mock XR Device. This is setup such that the runtime can query and receive
// fake data from the runtime, and tests can customize this and inspect any
// submitted frames.
// Please refer to xr_browser_tests.md for a description of the threading model.
// Due to this, it is important to document expectations for each method on
// where it is expecting to be called from via use of the sequence checkers.
class MockXRDeviceHookBase : public device_test::mojom::XRTestHook {
 public:
  MockXRDeviceHookBase();
  ~MockXRDeviceHookBase() override;

  // device_test::mojom::XRTestHook
  void OnFrameSubmitted(
      std::vector<device_test::mojom::ViewDataPtr> views,
      device_test::mojom::XRTestHook::OnFrameSubmittedCallback callback) final;
  void WaitGetDeviceConfig(
      device_test::mojom::XRTestHook::WaitGetDeviceConfigCallback callback)
      override;
  void WaitGetPresentingPose(
      device_test::mojom::XRTestHook::WaitGetPresentingPoseCallback callback)
      override;
  void WaitGetMagicWindowPose(
      device_test::mojom::XRTestHook::WaitGetMagicWindowPoseCallback callback)
      override;
  void WaitGetControllerRoleForTrackedDeviceIndex(
      unsigned int index,
      device_test::mojom::XRTestHook::
          WaitGetControllerRoleForTrackedDeviceIndexCallback callback) override;
  void WaitGetTrackedDeviceClass(
      unsigned int index,
      device_test::mojom::XRTestHook::WaitGetTrackedDeviceClassCallback
          callback) override;
  void WaitGetControllerData(
      unsigned int index,
      device_test::mojom::XRTestHook::WaitGetControllerDataCallback callback)
      override;
  void WaitGetEventData(device_test::mojom::XRTestHook::WaitGetEventDataCallback
                            callback) override;
  void WaitGetCanCreateSession(
      device_test::mojom::XRTestHook::WaitGetCanCreateSessionCallback callback)
      override;

  // MockXRDeviceHookBase
  void TerminateDeviceServiceProcessForTesting();
  unsigned int ConnectController(
      const device::ControllerFrameData& initial_data);
  void UpdateController(unsigned int index,
                        const device::ControllerFrameData& updated_data);
  void DisconnectController(unsigned int index);
  device::ControllerFrameData CreateValidController(
      device::ControllerRole role);
  void PopulateEvent(device_test::mojom::EventData data);
  void StopHooking();
  void SetCanCreateSession(bool can_create_session);
  uint32_t GetFrameCount() { return frame_count_; }
  void WaitNumFrames(uint32_t num_frames);
  void WaitForTotalFrameCount(uint32_t total_count);

 protected:
  // This allows subclasses to process the submitted frame. This method will be
  // called *after* the frame count has been incremented but *before* any
  // potenital wait loop is signaled.
  virtual void ProcessSubmittedFrameUnlocked(
      std::vector<device_test::mojom::ViewDataPtr> views) {}

  SEQUENCE_CHECKER(mock_device_sequence_);
  SEQUENCE_CHECKER(main_sequence_);
  base::Lock lock_;
  std::unique_ptr<base::Thread> thread_;

  std::array<device_test::mojom::TrackedDeviceClass, device::kMaxTrackedDevices>
      tracked_classes_ GUARDED_BY(lock_);
  base::flat_map<unsigned int, device::ControllerFrameData> controller_data_map_
      GUARDED_BY(lock_);
  std::queue<device_test::mojom::EventData> event_data_queue_ GUARDED_BY(lock_);

 private:
  mojo::Receiver<device_test::mojom::XRTestHook> receiver_{this};
  mojo::Remote<device_test::mojom::XRServiceTestHook> service_test_hook_;
  std::atomic_bool can_create_session_ = true;
  std::atomic_uint32_t frame_count_ = 0;
  std::atomic_uint32_t target_frame_count_ = 0;

  // Used to track both if `wait_loop_` is valid in a thread-safe manner or if
  // it has already had quit signaled on it, since `AnyQuitCalled` won't update
  // until the `Quit` task has posted to the main thread.
  std::atomic_bool can_signal_wait_loop_ = false;

  std::unique_ptr<base::RunLoop> wait_loop_ = nullptr;
};

#endif  // CHROME_BROWSER_VR_TEST_MOCK_XR_DEVICE_HOOK_BASE_H_
