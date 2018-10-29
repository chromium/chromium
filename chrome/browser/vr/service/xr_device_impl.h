// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SERVICE_XR_DEVICE_IMPL_H_
#define CHROME_BROWSER_VR_SERVICE_XR_DEVICE_IMPL_H_

#include <map>
#include <memory>

#include "base/macros.h"

#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"

namespace content {
class RenderFrameHost;
class WebContents;
}

namespace device {
class VRDisplayImpl;
}  // namespace device

namespace vr {

class BrowserXRRuntime;

// The browser-side host for a device::VRDisplayImpl. Controls access to VR
// APIs like poses and presentation.
class XRDeviceImpl : public device::mojom::XRDevice {
 public:
  XRDeviceImpl(content::RenderFrameHost* render_frame_host,
               device::mojom::XRDeviceRequest device_request);
  ~XRDeviceImpl() override;

  // device::mojom::XRDevice
  void RequestSession(
      device::mojom::XRSessionOptionsPtr options,
      bool triggered_by_displayactive,
      device::mojom::XRDevice::RequestSessionCallback callback) override;
  void SupportsSession(
      device::mojom::XRSessionOptionsPtr options,
      device::mojom::XRDevice::SupportsSessionCallback callback) override;
  void ExitPresent() override;
  // device::mojom::XRDevice WebVR compatibility functions
  void GetImmersiveVRDisplayInfo(
      device::mojom::XRDevice::GetImmersiveVRDisplayInfoCallback callback)
      override;

  void SetListeningForActivate(device::mojom::VRDisplayClientPtr client);

  void SetInFocusedFrame(bool in_focused_frame);

  // Notifications/calls from BrowserXRRuntime:
  void RuntimesChanged();
  void OnExitPresent();
  void OnBlur();
  void OnFocus();
  void OnActivate(device::mojom::VRDisplayEventReason reason,
                  base::OnceCallback<void(bool)> on_handled);
  void OnDeactivate(device::mojom::VRDisplayEventReason reason);
  bool ListeningForActivate() { return !!client_; }
  bool InFocusedFrame() { return in_focused_frame_; }

  base::WeakPtr<XRDeviceImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  content::WebContents* GetWebContents();

 private:
  void ReportRequestPresent();
  bool IsAnotherHostPresenting();

  bool InternalSupportsSession(device::mojom::XRSessionOptions* options);
  void OnNonImmersiveSessionCreated(
      device::mojom::XRDevice::RequestSessionCallback callback,
      device::mojom::XRSessionPtr session,
      device::mojom::XRSessionControllerPtr controller);
  void OnSessionCreated(
      device::mojom::XRDevice::RequestSessionCallback callback,
      device::mojom::XRSessionPtr session);

  // TODO(https://crbug.com/837538): Instead, check before returning this
  // object.
  bool IsSecureContextRequirementSatisfied();

  bool in_focused_frame_ = false;

  content::RenderFrameHost* render_frame_host_;
  mojo::Binding<device::mojom::XRDevice> binding_;
  mojo::InterfacePtrSet<device::mojom::XRSessionClient> session_clients_;
  // This is required for WebVR 1.1 backwards compatibility.
  device::mojom::VRDisplayClientPtr client_;

  mojo::InterfacePtrSet<device::mojom::XRSessionController>
      magic_window_controllers_;
  int next_key_ = 0;

  base::WeakPtrFactory<XRDeviceImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(XRDeviceImpl);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SERVICE_XR_DEVICE_IMPL_H_
