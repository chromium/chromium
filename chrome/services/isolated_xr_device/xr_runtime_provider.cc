// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/isolated_xr_device/xr_runtime_provider.h"

#include "base/bind.h"
#include "base/trace_event/trace_event.h"
#include "chrome/common/chrome_features.h"
#include "device/vr/buildflags/buildflags.h"
#include "device/vr/vr_device_base.h"

#if BUILDFLAG(ENABLE_OPENVR)
#include "device/vr/openvr/openvr_device.h"
#endif

#if BUILDFLAG(ENABLE_OCULUS_VR)
#include "device/vr/oculus/oculus_device.h"
#endif

#if BUILDFLAG(ENABLE_WINDOWS_MR)
#include "device/vr/windows_mixed_reality/mixed_reality_device.h"
#include "device/vr/windows_mixed_reality/mixed_reality_statics.h"
#endif

#if BUILDFLAG(ENABLE_OPENXR)
#include "device/vr/openxr/openxr_device.h"
#include "device/vr/openxr/openxr_statics.h"
#endif

enum class IsolatedXRRuntimeProvider::RuntimeStatus {
  kEnable,
  kDisable,
};

namespace {
// Poll for device add/remove every 5 seconds.
constexpr base::TimeDelta kTimeBetweenPollingEvents =
    base::TimeDelta::FromSecondsD(5);

template <typename VrDeviceT>
std::unique_ptr<VrDeviceT> EnableRuntime(
    device::mojom::IsolatedXRRuntimeProviderClient* client) {
  auto device = std::make_unique<VrDeviceT>();
  TRACE_EVENT_INSTANT1("xr", "HardwareAdded", TRACE_EVENT_SCOPE_THREAD, "id",
                       static_cast<int>(device->GetId()));
  // "Device" here refers to a runtime + hardware pair, not necessarily
  // a physical device.
  client->OnDeviceAdded(device->BindXRRuntime(), device->BindCompositorHost(),
                        device->GetId());
  return device;
}

template <typename VrDeviceT>
void DisableRuntime(device::mojom::IsolatedXRRuntimeProviderClient* client,
                    std::unique_ptr<VrDeviceT> device) {
  TRACE_EVENT_INSTANT1("xr", "HardwareRemoved", TRACE_EVENT_SCOPE_THREAD, "id",
                       static_cast<int>(device->GetId()));
  // "Device" here refers to a runtime + hardware pair, not necessarily physical
  // device.
  client->OnDeviceRemoved(device->GetId());
}

template <typename VrHardwareT>
void SetRuntimeStatus(device::mojom::IsolatedXRRuntimeProviderClient* client,
                      IsolatedXRRuntimeProvider::RuntimeStatus status,
                      std::unique_ptr<VrHardwareT>* out_device) {
  if (status == IsolatedXRRuntimeProvider::RuntimeStatus::kEnable &&
      !*out_device) {
    *out_device = EnableRuntime<VrHardwareT>(client);
  } else if (status == IsolatedXRRuntimeProvider::RuntimeStatus::kDisable &&
             *out_device) {
    DisableRuntime(client, std::move(*out_device));
  }
}

}  // namespace

// This function is called periodically to check the availability of hardware
// backed by the various supported VR runtimes. Only one "device" (hardware +
// runtime) should be enabled at once, so this chooses the most preferred among
// available options.
void IsolatedXRRuntimeProvider::PollForDeviceChanges() {
  bool preferred_device_enabled = false;

  // If none of the following runtimes are enabled,
  // we'll get an error for 'preferred_device_enabled' being unused.
  // Cast it to void (nop) here to mitigate that error.
  (void)preferred_device_enabled;

#if BUILDFLAG(ENABLE_OPENXR)
  if (!preferred_device_enabled && IsOpenXrHardwareAvailable()) {
    SetOpenXrRuntimeStatus(RuntimeStatus::kEnable);
    preferred_device_enabled = true;
  } else {
    SetOpenXrRuntimeStatus(RuntimeStatus::kDisable);
  }
#endif

#if BUILDFLAG(ENABLE_WINDOWS_MR)
  if (!preferred_device_enabled && IsWMRHardwareAvailable()) {
    SetWMRRuntimeStatus(RuntimeStatus::kEnable);
    preferred_device_enabled = true;
  } else {
    SetWMRRuntimeStatus(RuntimeStatus::kDisable);
  }
#endif

#if BUILDFLAG(ENABLE_OCULUS_VR)
  if (!preferred_device_enabled && IsOculusVrHardwareAvailable()) {
    SetOculusVrRuntimeStatus(RuntimeStatus::kEnable);
    preferred_device_enabled = true;
  } else {
    SetOculusVrRuntimeStatus(RuntimeStatus::kDisable);
  }
#endif

#if BUILDFLAG(ENABLE_OPENVR)
  if (!preferred_device_enabled && IsOpenVrHardwareAvailable()) {
    SetOpenVrRuntimeStatus(RuntimeStatus::kEnable);
    preferred_device_enabled = true;
  } else {
    SetOpenVrRuntimeStatus(RuntimeStatus::kDisable);
  }
#endif

  // Schedule this function to run again later.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&IsolatedXRRuntimeProvider::PollForDeviceChanges,
                     weak_ptr_factory_.GetWeakPtr()),
      kTimeBetweenPollingEvents);
}

void IsolatedXRRuntimeProvider::SetupPollingForDeviceChanges() {
  bool any_runtimes_available = false;

#if BUILDFLAG(ENABLE_OCULUS_VR)
  if (base::FeatureList::IsEnabled(features::kOculusVR)) {
    should_check_oculus_ = device::OculusDevice::IsApiAvailable();
    any_runtimes_available |= should_check_oculus_;
  }
#endif

#if BUILDFLAG(ENABLE_OPENVR)
  if (base::FeatureList::IsEnabled(features::kOpenVR)) {
    should_check_openvr_ = device::OpenVRDevice::IsApiAvailable();
    any_runtimes_available |= should_check_openvr_;
  }
#endif

#if BUILDFLAG(ENABLE_WINDOWS_MR)
  if (base::FeatureList::IsEnabled(features::kWindowsMixedReality)) {
    wmr_statics_ = device::MixedRealityDeviceStatics::CreateInstance();
    should_check_wmr_ = wmr_statics_->IsApiAvailable();
    any_runtimes_available |= should_check_wmr_;
  }
#endif

#if BUILDFLAG(ENABLE_OPENXR)
  if (base::FeatureList::IsEnabled(features::kOpenXR)) {
    openxr_statics_ = std::make_unique<device::OpenXrStatics>();
    should_check_openxr_ = openxr_statics_->IsApiAvailable();
    any_runtimes_available |= should_check_openxr_;
  }
#endif

  // Begin polling for devices
  if (any_runtimes_available) {
    PollForDeviceChanges();
  }
}

void IsolatedXRRuntimeProvider::RequestDevices(
    mojo::PendingRemote<device::mojom::IsolatedXRRuntimeProviderClient>
        client) {
  // Start polling to detect devices being added/removed.
  client_.Bind(std::move(client));
  SetupPollingForDeviceChanges();
  client_->OnDevicesEnumerated();
}

#if BUILDFLAG(ENABLE_OCULUS_VR)
bool IsolatedXRRuntimeProvider::IsOculusVrHardwareAvailable() {
  return should_check_oculus_ &&
         ((oculus_device_ && oculus_device_->IsAvailable()) ||
          device::OculusDevice::IsHwAvailable());
}

void IsolatedXRRuntimeProvider::SetOculusVrRuntimeStatus(RuntimeStatus status) {
  SetRuntimeStatus(client_.get(), status, &oculus_device_);
}
#endif  // BUILDFLAG(ENABLE_OCULUS_VR)

#if BUILDFLAG(ENABLE_OPENVR)
bool IsolatedXRRuntimeProvider::IsOpenVrHardwareAvailable() {
  return should_check_openvr_ &&
         ((openvr_device_ && openvr_device_->IsAvailable()) ||
          device::OpenVRDevice::IsHwAvailable());
}

void IsolatedXRRuntimeProvider::SetOpenVrRuntimeStatus(RuntimeStatus status) {
  SetRuntimeStatus(client_.get(), status, &openvr_device_);
}
#endif  // BUILDFLAG(ENABLE_OPENVR)

#if BUILDFLAG(ENABLE_WINDOWS_MR)
bool IsolatedXRRuntimeProvider::IsWMRHardwareAvailable() {
  return should_check_wmr_ && wmr_statics_->IsHardwareAvailable();
}

void IsolatedXRRuntimeProvider::SetWMRRuntimeStatus(RuntimeStatus status) {
  SetRuntimeStatus(client_.get(), status, &wmr_device_);
}
#endif  // BUILDFLAG(ENABLE_WINDOWS_MR)

#if BUILDFLAG(ENABLE_OPENXR)
bool IsolatedXRRuntimeProvider::IsOpenXrHardwareAvailable() {
  return should_check_openxr_ && openxr_statics_->IsHardwareAvailable();
}

void IsolatedXRRuntimeProvider::SetOpenXrRuntimeStatus(RuntimeStatus status) {
  SetRuntimeStatus(client_.get(), status, &openxr_device_);
}
#endif  // BUILDFLAG(ENABLE_OPENXR)

IsolatedXRRuntimeProvider::IsolatedXRRuntimeProvider() = default;

IsolatedXRRuntimeProvider::~IsolatedXRRuntimeProvider() {
#if BUILDFLAG(ENABLE_WINDOWS_MR)
  // Explicitly null out wmr_device_ to clean up any COM objects that depend
  // on being RoInitialized
  wmr_device_ = nullptr;
#endif  // BUILDFLAG(ENABLE_WINDOWS_MR)
}
