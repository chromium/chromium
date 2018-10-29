// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/service/xr_runtime_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "chrome/browser/vr/service/browser_xr_runtime.h"
#include "chrome/common/chrome_features.h"
#include "content/public/common/content_features.h"
#include "content/public/common/service_manager_connection.h"
#include "device/vr/buildflags/buildflags.h"
#include "device/vr/orientation/orientation_device_provider.h"
#include "device/vr/vr_device_provider.h"

#if defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_ARCORE)
#include "device/vr/android/arcore/arcore_device_provider_factory.h"
#endif

#include "device/vr/android/gvr/gvr_device_provider.h"
#endif

#if BUILDFLAG(ENABLE_ISOLATED_XR_SERVICE)
// We are hosting Oculus and OpenVR in a separate process
#include "chrome/browser/vr/service/isolated_device_provider.h"
#else
// We are hosting Oculus and OpenVR in the browser process if enabled.

#if BUILDFLAG(ENABLE_OPENVR)
#include "device/vr/openvr/openvr_device_provider.h"
#endif

#if BUILDFLAG(ENABLE_OCULUS_VR)
#include "device/vr/oculus/oculus_device_provider.h"
#endif

#endif  // BUILDFLAG(ENABLE_ISOLATED_XR_SERVICE)

namespace vr {

namespace {
XRRuntimeManager* g_xr_runtime_manager = nullptr;
}  // namespace

XRRuntimeManager::~XRRuntimeManager() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  g_xr_runtime_manager = nullptr;
}

XRRuntimeManager* XRRuntimeManager::GetInstance() {
  if (!g_xr_runtime_manager) {
    // Register VRDeviceProviders for the current platform
    ProviderList providers;

#if defined(OS_ANDROID)
#if BUILDFLAG(ENABLE_ARCORE)
    if (base::FeatureList::IsEnabled(features::kWebXrHitTest)) {
      providers.emplace_back(device::ArCoreDeviceProviderFactory::Create());
    }
#endif

    providers.emplace_back(std::make_unique<device::GvrDeviceProvider>());
#endif

#if BUILDFLAG(ENABLE_ISOLATED_XR_SERVICE)
    providers.emplace_back(std::make_unique<vr::IsolatedVRDeviceProvider>());
#else
#if BUILDFLAG(ENABLE_OPENVR)
    if (base::FeatureList::IsEnabled(features::kOpenVR))
      providers.emplace_back(std::make_unique<device::OpenVRDeviceProvider>());
#endif

#if BUILDFLAG(ENABLE_OCULUS_VR)
    // For now, only use the Oculus when OpenVR is not enabled.
    // TODO(billorr): Add more complicated logic to avoid routing Oculus devices
    // through OpenVR.
    if (base::FeatureList::IsEnabled(features::kOculusVR) &&
        providers.size() == 0)
      providers.emplace_back(
          std::make_unique<device::OculusVRDeviceProvider>());
#endif
#endif  // ENABLE_ISOLATED_XR_SERVICE

    if (base::FeatureList::IsEnabled(features::kWebXrOrientationSensorDevice)) {
      content::ServiceManagerConnection* connection =
          content::ServiceManagerConnection::GetForProcess();
      if (connection) {
        providers.emplace_back(
            std::make_unique<device::VROrientationDeviceProvider>(
                connection->GetConnector()));
      }
    }

    // The constructor sets g_xr_runtime_manager, which is cleaned up when
    // RemoveService is called, when the last active VRServiceImpl is destroyed.
    new XRRuntimeManager(std::move(providers));
  }
  return g_xr_runtime_manager;
}

bool XRRuntimeManager::HasInstance() {
  return g_xr_runtime_manager != nullptr;
}

void XRRuntimeManager::RecordVrStartupHistograms() {
#if BUILDFLAG(ENABLE_OPENVR) && !BUILDFLAG(ENABLE_ISOLATED_XR_SERVICE)
  device::OpenVRDeviceProvider::RecordRuntimeAvailability();
#endif
}

void XRRuntimeManager::AddService(VRServiceImpl* service) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Loop through any currently active devices and send Connected messages to
  // the service. Future devices that come online will send a Connected message
  // when they are created.
  InitializeProviders();

  if (AreAllProvidersInitialized())
    service->InitializationComplete();

  services_.insert(service);
}

void XRRuntimeManager::RemoveService(VRServiceImpl* service) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  services_.erase(service);

  if (services_.empty()) {
    // Delete the device manager when it has no active connections.
    delete g_xr_runtime_manager;
  }
}

BrowserXRRuntime* XRRuntimeManager::GetRuntime(device::mojom::XRDeviceId id) {
  auto it = runtimes_.find(id);
  if (it == runtimes_.end())
    return nullptr;

  return it->second.get();
}

BrowserXRRuntime* XRRuntimeManager::GetRuntimeForOptions(
    device::mojom::XRSessionOptions* options) {
  // Examine options to determine which device provider we should use.
  if (options->immersive && !options->provide_passthrough_camera) {
    return GetImmersiveRuntime();
  } else if (options->provide_passthrough_camera && !options->immersive) {
    return GetRuntime(device::mojom::XRDeviceId::ARCORE_DEVICE_ID);
  } else if (!options->provide_passthrough_camera && !options->immersive) {
    // Non immersive session.
    // Try the orientation provider if it exists.
    auto* orientation_runtime =
        GetRuntime(device::mojom::XRDeviceId::ORIENTATION_DEVICE_ID);
    if (orientation_runtime) {
      return orientation_runtime;
    }

    // Otherwise fall back to immersive providers.
    return GetImmersiveRuntime();
  }
  return nullptr;
}

BrowserXRRuntime* XRRuntimeManager::GetImmersiveRuntime() {
#if defined(OS_ANDROID)
  auto* gvr = GetRuntime(device::mojom::XRDeviceId::GVR_DEVICE_ID);
  if (gvr)
    return gvr;
#endif

#if BUILDFLAG(ENABLE_OPENVR)
  auto* openvr = GetRuntime(device::mojom::XRDeviceId::OPENVR_DEVICE_ID);
  if (openvr)
    return openvr;
#endif

#if BUILDFLAG(ENABLE_OCULUS_VR)
  auto* oculus = GetRuntime(device::mojom::XRDeviceId::OCULUS_DEVICE_ID);
  if (oculus)
    return oculus;
#endif

  return nullptr;
}

device::mojom::VRDisplayInfoPtr XRRuntimeManager::GetCurrentVRDisplayInfo(
    XRDeviceImpl* device) {
  // Get an immersive_runtime device if there is one.
  auto* immersive_runtime = GetImmersiveRuntime();
  if (immersive_runtime) {
    // Listen to changes for this device.
    immersive_runtime->OnRendererDeviceAdded(device);

    // If we don't have display info for the immersive device, get display info
    // from a different device.
    if (!immersive_runtime->GetVRDisplayInfo()) {
      immersive_runtime = nullptr;
    }
  }

  // Get an AR device if there is one.
  device::mojom::XRSessionOptions options = {};
  options.provide_passthrough_camera = true;
  auto* ar_runtime = GetRuntimeForOptions(&options);
  if (ar_runtime) {
    // Listen to  changes for this device.
    ar_runtime->OnRendererDeviceAdded(device);
  }

  // If there is neither, use the generic non-immersive device.
  if (!ar_runtime && !immersive_runtime) {
    device::mojom::XRSessionOptions options = {};
    auto* non_immersive_runtime = GetRuntimeForOptions(&options);
    if (non_immersive_runtime) {
      // Listen to changes for this device.
      non_immersive_runtime->OnRendererDeviceAdded(device);
    }

    // If we don't have an AR or immersive device, return the generic non-
    // immersive device's DisplayInfo if we have it.
    return non_immersive_runtime ? non_immersive_runtime->GetVRDisplayInfo()
                                 : nullptr;
  }

  // Use the immersive or AR device. However, if we are using the immersive
  // device's info, and AR is supported, reflect that in capabilities.
  device::mojom::VRDisplayInfoPtr device_info =
      immersive_runtime ? immersive_runtime->GetVRDisplayInfo()
                        : ar_runtime->GetVRDisplayInfo();
  device_info->capabilities->can_provide_pass_through_images = !!ar_runtime;

  return device_info;
}

void XRRuntimeManager::OnRendererDeviceRemoved(XRDeviceImpl* device) {
  for (const auto& runtime : runtimes_) {
    runtime.second->OnRendererDeviceRemoved(device);
  }
}

bool XRRuntimeManager::IsOtherDevicePresenting(XRDeviceImpl* device) {
  DCHECK(device);

  auto* runtime = GetImmersiveRuntime();
  if (!runtime)
    return false;  // No immersive runtime to be presenting.

  XRDeviceImpl* presenting_device = runtime->GetPresentingRendererDevice();
  if (!presenting_device)
    return false;  // No XRDeviceImpl is presenting.

  // True if some other XRDeviceImpl is presenting.
  return (presenting_device != device);
}

bool XRRuntimeManager::HasAnyRuntime() {
  return runtimes_.size() > 0;
}

void XRRuntimeManager::SupportsSession(
    device::mojom::XRSessionOptionsPtr options,
    device::mojom::XRDevice::SupportsSessionCallback callback) {
  auto* runtime = GetRuntimeForOptions(options.get());

  if (!runtime) {
    std::move(callback).Run(false);
    return;
  }

  // TODO(http://crbug.com/842025): Pass supports session on to the device
  // runtimes.
  std::move(callback).Run(true);
}

XRRuntimeManager::XRRuntimeManager(ProviderList providers)
    : providers_(std::move(providers)) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(!g_xr_runtime_manager);
  g_xr_runtime_manager = this;
}

device::mojom::XRRuntime* XRRuntimeManager::GetRuntimeForTest(
    device::mojom::XRDeviceId id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DeviceRuntimeMap::iterator iter =
      runtimes_.find(static_cast<device::mojom::XRDeviceId>(id));
  if (iter == runtimes_.end())
    return nullptr;

  return iter->second->GetRuntime();
}

size_t XRRuntimeManager::NumberOfConnectedServices() {
  return services_.size();
}

void XRRuntimeManager::InitializeProviders() {
  if (providers_initialized_)
    return;

  for (const auto& provider : providers_) {
    provider->Initialize(
        base::BindRepeating(&XRRuntimeManager::AddRuntime,
                            base::Unretained(this)),
        base::BindRepeating(&XRRuntimeManager::RemoveRuntime,
                            base::Unretained(this)),
        base::BindOnce(&XRRuntimeManager::OnProviderInitialized,
                       base::Unretained(this)));
  }

  providers_initialized_ = true;
}

void XRRuntimeManager::OnProviderInitialized() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ++num_initialized_providers_;
  if (AreAllProvidersInitialized()) {
    for (VRServiceImpl* service : services_)
      service->InitializationComplete();
  }
}

bool XRRuntimeManager::AreAllProvidersInitialized() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return num_initialized_providers_ == providers_.size();
}

void XRRuntimeManager::AddRuntime(device::mojom::XRDeviceId id,
                                  device::mojom::VRDisplayInfoPtr info,
                                  device::mojom::XRRuntimePtr runtime) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(runtimes_.find(id) == runtimes_.end());

  runtimes_[id] =
      std::make_unique<BrowserXRRuntime>(std::move(runtime), std::move(info));
  for (VRServiceImpl* service : services_)
    service->RuntimesChanged();
}

void XRRuntimeManager::RemoveRuntime(device::mojom::XRDeviceId id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = runtimes_.find(id);
  DCHECK(it != runtimes_.end());

  // Remove the device from runtimes_ before notifying services that it was
  // removed, since they will query for devices in RemoveRuntime.
  std::unique_ptr<BrowserXRRuntime> removed_device = std::move(it->second);
  runtimes_.erase(it);

  for (VRServiceImpl* service : services_)
    service->RuntimesChanged();
}

}  // namespace vr
