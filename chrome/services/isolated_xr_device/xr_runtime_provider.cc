// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/isolated_xr_device/xr_runtime_provider.h"
#include "chrome/common/chrome_features.h"
#include "device/vr/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_OPENVR)
#include "device/vr/openvr/openvr_device.h"
#endif

#if BUILDFLAG(ENABLE_OCULUS_VR)
#include "device/vr/oculus/oculus_device.h"
#endif

void IsolatedXRRuntimeProvider::RequestDevices(
    device::mojom::IsolatedXRRuntimeProviderClientPtr client) {
#if BUILDFLAG(ENABLE_OCULUS_VR)
  if (base::FeatureList::IsEnabled(features::kOculusVR)) {
    oculus_device_ = std::make_unique<device::OculusDevice>();
    if (!oculus_device_->IsInitialized()) {
      oculus_device_ = nullptr;
    } else {
      client->OnDeviceAdded(oculus_device_->BindXRRuntimePtr(),
                            oculus_device_->BindGamepadFactory(),
                            oculus_device_->BindCompositorHost(),
                            oculus_device_->GetVRDisplayInfo());
    }
  }
#endif

#if BUILDFLAG(ENABLE_OPENVR)
  if (base::FeatureList::IsEnabled(features::kOpenVR)) {
    openvr_device_ = std::make_unique<device::OpenVRDevice>();
    if (!openvr_device_->IsInitialized()) {
      openvr_device_ = nullptr;
    } else {
      client->OnDeviceAdded(openvr_device_->BindXRRuntimePtr(),
                            openvr_device_->BindGamepadFactory(),
                            openvr_device_->BindCompositorHost(),
                            openvr_device_->GetVRDisplayInfo());
    }
  }
#endif

  client->OnDevicesEnumerated();
  client_ = std::move(client);
}

IsolatedXRRuntimeProvider::IsolatedXRRuntimeProvider(
    std::unique_ptr<service_manager::ServiceContextRef> service_ref)
    : service_ref_(std::move(service_ref)) {}

IsolatedXRRuntimeProvider::~IsolatedXRRuntimeProvider() {}
