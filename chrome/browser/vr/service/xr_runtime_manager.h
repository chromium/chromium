// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SERVICE_XR_RUNTIME_MANAGER_H_
#define CHROME_BROWSER_VR_SERVICE_XR_RUNTIME_MANAGER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "chrome/browser/vr/service/vr_service_impl.h"
#include "chrome/browser/vr/vr_export.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace device {
class VRDeviceProvider;
}

namespace vr {

class BrowserXRRuntime;

// Singleton used to provide the platform's VR devices to VRServiceImpl
// instances.
class VR_EXPORT XRRuntimeManager {
 public:
  virtual ~XRRuntimeManager();

  // Returns the XRRuntimeManager singleton.
  static XRRuntimeManager* GetInstance();
  static bool HasInstance();
  static void RecordVrStartupHistograms();

  // Adds a listener for runtime manager events. XRRuntimeManager does not own
  // this object.
  void AddService(VRServiceImpl* service);
  void RemoveService(VRServiceImpl* service);

  BrowserXRRuntime* GetRuntime(device::mojom::XRDeviceId id);
  BrowserXRRuntime* GetRuntimeForOptions(
      device::mojom::XRSessionOptions* options);
  BrowserXRRuntime* GetImmersiveRuntime();
  device::mojom::VRDisplayInfoPtr GetCurrentVRDisplayInfo(XRDeviceImpl* device);
  void OnRendererDeviceRemoved(XRDeviceImpl* device);

  // Returns true if another device is presenting. Returns false if this device
  // is presenting, or if nobody is presenting.
  bool IsOtherDevicePresenting(XRDeviceImpl* device);
  bool HasAnyRuntime();

  void SupportsSession(
      device::mojom::XRSessionOptionsPtr options,
      device::mojom::XRDevice::SupportsSessionCallback callback);

 protected:
  using ProviderList = std::vector<std::unique_ptr<device::VRDeviceProvider>>;

  // Constructor also used by tests to supply an arbitrary list of providers, so
  // make it protected rather than private.
  explicit XRRuntimeManager(ProviderList providers);

  // Used by tests to check on device state.
  // TODO: Use XRDeviceId as appropriate.
  device::mojom::XRRuntime* GetRuntimeForTest(device::mojom::XRDeviceId id);

  size_t NumberOfConnectedServices();

 private:
  void InitializeProviders();
  void OnProviderInitialized();
  bool AreAllProvidersInitialized();

  void AddRuntime(device::mojom::XRDeviceId id,
                  device::mojom::VRDisplayInfoPtr info,
                  device::mojom::XRRuntimePtr runtime);
  void RemoveRuntime(device::mojom::XRDeviceId id);

  ProviderList providers_;

  // VRDevices are owned by their providers, each correspond to a
  // BrowserXRRuntime that is owned by XRRuntimeManager.
  using DeviceRuntimeMap = base::small_map<
      std::map<device::mojom::XRDeviceId, std::unique_ptr<BrowserXRRuntime>>>;
  DeviceRuntimeMap runtimes_;

  bool providers_initialized_ = false;
  size_t num_initialized_providers_ = 0;

  std::set<VRServiceImpl*> services_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(XRRuntimeManager);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SERVICE_XR_RUNTIME_MANAGER_H_
