// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_ISOLATED_XR_DEVICE_XR_TEST_HOOK_WRAPPER_H_
#define CHROME_SERVICES_ISOLATED_XR_DEVICE_XR_TEST_HOOK_WRAPPER_H_

#include "device/vr/openvr/test/test_hook.h"
#include "device/vr/public/mojom/browser_test_interfaces.mojom.h"

namespace device {

// Wraps a mojo test hook to implement the OpenVRTestHook C++ interface.  Our
// use of OpenVR is single-threaded at a time, and we initialize/uninitialize
// as we switch between immersive and non-immersive sessions.
// The mojo pointer is thread-affine, but we can keep the same mojo connection
// by getting its PtrInfo so we temporarily make the test hook mojo pointer
// live on the thread that we are using OpenVR on while OpenVR is initialized.
class XRTestHookWrapper : public OpenVRTestHook {
 public:
  explicit XRTestHookWrapper(device_test::mojom::XRTestHookPtrInfo hook_info);
  virtual ~XRTestHookWrapper();

  void OnFrameSubmitted(SubmittedFrameData frame_data) override;
  DeviceConfig WaitGetDeviceConfig() override;
  PoseFrameData WaitGetPresentingPose() override;
  PoseFrameData WaitGetMagicWindowPose() override;
  ControllerRole WaitGetControllerRoleForTrackedDeviceIndex(
      unsigned int index) override;
  TrackedDeviceClass WaitGetTrackedDeviceClass(unsigned int index) override;
  ControllerFrameData WaitGetControllerData(unsigned int index) override;
  void AttachCurrentThread() override;
  void DetachCurrentThread() override;

  scoped_refptr<base::SingleThreadTaskRunner> GetBoundTaskRunner();

 private:
  device_test::mojom::XRTestHookPtr hook_;
  device_test::mojom::XRTestHookPtrInfo hook_info_;
  scoped_refptr<base::SingleThreadTaskRunner> current_task_runner_;
};

}  // namespace device

#endif  // CHROME_SERVICES_ISOLATED_XR_DEVICE_XR_TEST_HOOK_WRAPPER_H_
