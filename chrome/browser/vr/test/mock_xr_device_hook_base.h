// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_MOCK_XR_DEVICE_HOOK_BASE_H_
#define CHROME_BROWSER_VR_TEST_MOCK_XR_DEVICE_HOOK_BASE_H_

#include <queue>

#include "base/containers/flat_map.h"
#include "device/vr/public/mojom/browser_test_interfaces.mojom-forward.h"
#include "device/vr/test/test_hook.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class MockXRDeviceHookBase : public device_test::mojom::XRTestHook {
 public:
  MockXRDeviceHookBase();
  ~MockXRDeviceHookBase() override;

  // device_test::mojom::XRTestHook
  void OnFrameSubmitted(std::vector<device_test::mojom::ViewDataPtr> views,
                        device_test::mojom::XRTestHook::OnFrameSubmittedCallback
                            callback) override;
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

 protected:
  device_test::mojom::TrackedDeviceClass
      tracked_classes_[device::kMaxTrackedDevices];
  base::flat_map<unsigned int, device::ControllerFrameData>
      controller_data_map_;
  std::queue<device_test::mojom::EventData> event_data_queue_;

 private:
  mojo::Receiver<device_test::mojom::XRTestHook> receiver_{this};
  mojo::Remote<device_test::mojom::XRServiceTestHook> service_test_hook_;
  bool can_create_session_ = true;
};

#endif  // CHROME_BROWSER_VR_TEST_MOCK_XR_DEVICE_HOOK_BASE_H_
