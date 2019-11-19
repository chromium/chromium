// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SERVICE_XR_RUNTIME_MANAGER_H_
#define CHROME_BROWSER_VR_SERVICE_XR_RUNTIME_MANAGER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "chrome/browser/vr/service/vr_service_impl.h"
#include "chrome/browser/vr/service/xr_runtime_manager_observer.h"
#include "chrome/browser/vr/vr_export.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace device {
class VRDeviceProvider;
}

namespace vr {

class BrowserXRRuntime;
class XRRuntimeManagerTest;

// Singleton used to provide the platform's XR Runtimes to VRServiceImpl
// instances.
class VR_EXPORT XRRuntimeManager : public base::RefCounted<XRRuntimeManager> {
 public:
  friend base::RefCounted<XRRuntimeManager>;
  static constexpr auto kRefCountPreference =
      base::subtle::kStartRefCountFromOneTag;

  friend XRRuntimeManagerTest;

  // Returns a pointer to the XRRuntimeManager singleton.
  // If The singleton is not currently instantiated, this instantiates it with
  // the built-in set of providers.
  // The singleton will persist until all pointers have been dropped.
  static scoped_refptr<XRRuntimeManager> GetOrCreateInstance();

  // If there is no-one currently using the XRRuntimeManager, then it won't be
  // instantiated.
  static bool HasInstance();

  // Provides access to the XRRuntimeManager singleton without causing
  // reference count churn. This method does not extend the lifetime of the
  // singleton, so you should be careful with the lifetime of this reference.
  static XRRuntimeManager* GetInstanceIfCreated();

  // Statics for global observers.
  static void AddObserver(XRRuntimeManagerObserver* observer);
  static void RemoveObserver(XRRuntimeManagerObserver* observer);

  static void ExitImmersivePresentation();
  static void RecordVrStartupHistograms();

  // Adds a listener for runtime manager events. XRRuntimeManager does not own
  // this object.
  void AddService(VRServiceImpl* service);
  void RemoveService(VRServiceImpl* service);

  BrowserXRRuntime* GetRuntime(device::mojom::XRDeviceId id);
  bool HasRuntime(device::mojom::XRDeviceId id);
  BrowserXRRuntime* GetRuntimeForOptions(
      device::mojom::XRSessionOptions* options);

  // Gets the system default immersive-vr runtime if available.
  BrowserXRRuntime* GetImmersiveVrRuntime();

  // Gets the system default immersive-ar runtime if available.
  BrowserXRRuntime* GetImmersiveArRuntime();

  // Gets the runtime matching a currently-active immersive session, if any.
  // This is either the VR or AR runtime, or null if there's no matching
  // runtime or if there's no active immersive session.
  BrowserXRRuntime* GetCurrentlyPresentingImmersiveRuntime();

  device::mojom::VRDisplayInfoPtr GetCurrentVRDisplayInfo(
      VRServiceImpl* service);

  // Returns true if another service is presenting. Returns false if this
  // service is presenting, or if nobody is presenting.
  bool IsOtherClientPresenting(VRServiceImpl* service);
  bool HasAnyRuntime();

  void SupportsSession(
      device::mojom::XRSessionOptionsPtr options,
      device::mojom::VRService::SupportsSessionCallback callback);

  template <typename Fn>
  void ForEachRuntime(Fn&& fn) {
    for (auto& rt : runtimes_) {
      fn(rt.second.get());
    }
  }

 private:
  using ProviderList = std::vector<std::unique_ptr<device::VRDeviceProvider>>;

  // Constructor also used by tests to supply an arbitrary list of providers
  static scoped_refptr<XRRuntimeManager> CreateInstance(ProviderList providers);

  // Used by tests to check on runtime state.
  device::mojom::XRRuntime* GetRuntimeForTest(device::mojom::XRDeviceId id);

  // Used by tests
  size_t NumberOfConnectedServices();

  explicit XRRuntimeManager(ProviderList providers);

  ~XRRuntimeManager();

  void InitializeProviders();
  void OnProviderInitialized();
  bool AreAllProvidersInitialized();

  void AddRuntime(device::mojom::XRDeviceId id,
                  device::mojom::VRDisplayInfoPtr info,
                  mojo::PendingRemote<device::mojom::XRRuntime> runtime);
  void RemoveRuntime(device::mojom::XRDeviceId id);

  ProviderList providers_;

  // XRRuntimes are owned by their providers, each correspond to a
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
