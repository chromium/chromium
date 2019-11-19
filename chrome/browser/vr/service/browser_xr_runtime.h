// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SERVICE_BROWSER_XR_RUNTIME_H_
#define CHROME_BROWSER_VR_SERVICE_BROWSER_XR_RUNTIME_H_

#include <set>
#include <vector>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chrome/browser/vr/service/vr_service_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class WebContents;
}

namespace vr {

// This interface is implemented by classes that wish to observer the state of
// the XR service for a particular runtime.  In particular, observers may
// currently know when the browser considers a WebContents presenting to an
// immersive headset.  Implementers of this interface will be called on the main
// browser thread.  Currently this is used on Windows to drive overlays.
class BrowserXRRuntimeObserver : public base::CheckedObserver {
 public:
  virtual void SetVRDisplayInfo(
      device::mojom::VRDisplayInfoPtr display_info) = 0;

  // The parameter |contents| is set when a page starts an immersive WebXR
  // session. There can only be at most one active immersive session for the
  // XRRuntime. Set to null when there is no active immersive session.
  virtual void SetWebXRWebContents(content::WebContents* contents) = 0;

  virtual void SetFramesThrottled(bool throttled) = 0;
};

// This class wraps a physical device's interfaces, and registers for events.
// There is one BrowserXRRuntime per physical device runtime.  It manages
// browser-side handling of state, like which VRServiceImpl is listening for
// device activation.
class BrowserXRRuntime : public device::mojom::XRRuntimeEventListener {
 public:
  using RequestSessionCallback =
      base::OnceCallback<void(device::mojom::XRSessionPtr)>;
  explicit BrowserXRRuntime(
      device::mojom::XRDeviceId id,
      mojo::PendingRemote<device::mojom::XRRuntime> runtime,
      device::mojom::VRDisplayInfoPtr info);
  ~BrowserXRRuntime() override;

  void ExitActiveImmersiveSession();
  bool SupportsFeature(device::mojom::XRSessionFeature feature) const;
  bool SupportsAllFeatures(
      const std::vector<device::mojom::XRSessionFeature>& features) const;

  bool SupportsCustomIPD() const;
  bool SupportsNonEmulatedHeight() const;

  device::mojom::XRRuntime* GetRuntime() { return runtime_.get(); }

  // Methods called by VRServiceImpl to interact with the runtime's device.
  void OnServiceAdded(VRServiceImpl* service);
  void OnServiceRemoved(VRServiceImpl* service);
  void ExitPresent(VRServiceImpl* service,
                   VRServiceImpl::ExitPresentCallback on_exited);
  void SetFramesThrottled(const VRServiceImpl* service, bool throttled);
  void RequestSession(VRServiceImpl* service,
                      const device::mojom::XRRuntimeSessionOptionsPtr& options,
                      RequestSessionCallback callback);
  VRServiceImpl* GetServiceWithActiveImmersiveSession() {
    return presenting_service_;
  }

  device::mojom::VRDisplayInfoPtr GetVRDisplayInfo() {
    return display_info_.Clone();
  }

  // Methods called to support metrics/overlays on Windows.
  void AddObserver(BrowserXRRuntimeObserver* observer) {
    observers_.AddObserver(observer);
    observer->SetVRDisplayInfo(display_info_.Clone());
  }
  void RemoveObserver(BrowserXRRuntimeObserver* observer) {
    observers_.RemoveObserver(observer);
  }
  device::mojom::XRDeviceId GetId() const { return id_; }

 private:
  // device::XRRuntimeEventListener
  void OnDisplayInfoChanged(
      device::mojom::VRDisplayInfoPtr vr_device_info) override;
  void OnExitPresent() override;
  void OnVisibilityStateChanged(
      device::mojom::XRVisibilityState visibility_state) override;

  void StopImmersiveSession(VRServiceImpl::ExitPresentCallback on_exited);
  void OnRequestSessionResult(
      base::WeakPtr<VRServiceImpl> service,
      device::mojom::XRRuntimeSessionOptionsPtr options,
      RequestSessionCallback callback,
      device::mojom::XRSessionPtr session,
      mojo::PendingRemote<device::mojom::XRSessionController>
          immersive_session_controller);
  void OnImmersiveSessionError();

  device::mojom::XRDeviceId id_;
  mojo::Remote<device::mojom::XRRuntime> runtime_;
  mojo::Remote<device::mojom::XRSessionController>
      immersive_session_controller_;

  std::set<VRServiceImpl*> services_;
  device::mojom::VRDisplayInfoPtr display_info_;

  VRServiceImpl* presenting_service_ = nullptr;

  mojo::AssociatedReceiver<device::mojom::XRRuntimeEventListener> receiver_{
      this};

  base::ObserverList<BrowserXRRuntimeObserver> observers_;

  base::WeakPtrFactory<BrowserXRRuntime> weak_ptr_factory_{this};
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SERVICE_BROWSER_XR_RUNTIME_H_
