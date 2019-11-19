// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_ISOLATED_XR_DEVICE_XR_RUNTIME_PROVIDER_H_
#define CHROME_SERVICES_ISOLATED_XR_DEVICE_XR_RUNTIME_PROVIDER_H_

#include <memory>

#include "device/vr/buildflags/buildflags.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace device {
class OculusDevice;
class OpenVRDevice;
class MixedRealityDevice;
class MixedRealityDeviceStatics;
class OpenXrDevice;
class OpenXrStatics;
}  // namespace device

class IsolatedXRRuntimeProvider
    : public device::mojom::IsolatedXRRuntimeProvider {
 public:
  IsolatedXRRuntimeProvider();
  ~IsolatedXRRuntimeProvider() final;

  void RequestDevices(
      mojo::PendingRemote<device::mojom::IsolatedXRRuntimeProviderClient>
          client) override;

  enum class RuntimeStatus;

 private:
  void PollForDeviceChanges();
  void SetupPollingForDeviceChanges();

#if BUILDFLAG(ENABLE_OCULUS_VR)
  bool IsOculusVrHardwareAvailable();
  void SetOculusVrRuntimeStatus(RuntimeStatus status);
  bool should_check_oculus_ = false;
  std::unique_ptr<device::OculusDevice> oculus_device_;
#endif

#if BUILDFLAG(ENABLE_OPENVR)
  bool IsOpenVrHardwareAvailable();
  void SetOpenVrRuntimeStatus(RuntimeStatus status);
  bool should_check_openvr_ = false;
  std::unique_ptr<device::OpenVRDevice> openvr_device_;
#endif

#if BUILDFLAG(ENABLE_WINDOWS_MR)
  bool IsWMRHardwareAvailable();
  void SetWMRRuntimeStatus(RuntimeStatus status);
  bool should_check_wmr_ = false;
  std::unique_ptr<device::MixedRealityDevice> wmr_device_;
  std::unique_ptr<device::MixedRealityDeviceStatics> wmr_statics_;
#endif

#if BUILDFLAG(ENABLE_OPENXR)
  bool IsOpenXrHardwareAvailable();
  void SetOpenXrRuntimeStatus(RuntimeStatus status);
  bool should_check_openxr_ = false;
  std::unique_ptr<device::OpenXrDevice> openxr_device_;
  std::unique_ptr<device::OpenXrStatics> openxr_statics_;
#endif

  mojo::Remote<device::mojom::IsolatedXRRuntimeProviderClient> client_;
  base::WeakPtrFactory<IsolatedXRRuntimeProvider> weak_ptr_factory_{this};
};

#endif  // CHROME_SERVICES_ISOLATED_XR_DEVICE_XR_RUNTIME_PROVIDER_H_
