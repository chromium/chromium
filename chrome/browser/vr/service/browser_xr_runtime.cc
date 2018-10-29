// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/service/browser_xr_runtime.h"

#include "chrome/browser/vr/service/xr_device_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/vr/vr_device.h"

namespace vr {

BrowserXRRuntime::BrowserXRRuntime(device::mojom::XRRuntimePtr runtime,
                                   device::mojom::VRDisplayInfoPtr display_info)
    : runtime_(std::move(runtime)),
      display_info_(std::move(display_info)),
      binding_(this),
      weak_ptr_factory_(this) {
  device::mojom::XRRuntimeEventListenerAssociatedPtr listener;
  binding_.Bind(mojo::MakeRequest(&listener));

  // Unretained is safe because we are calling through an InterfacePtr we own,
  // so we won't be called after runtime_ is destroyed.
  runtime_->ListenToDeviceChanges(
      listener.PassInterface(),
      base::BindOnce(&BrowserXRRuntime::OnInitialDevicePropertiesReceived,
                     base::Unretained(this)));
}

void BrowserXRRuntime::OnInitialDevicePropertiesReceived(
    device::mojom::VRDisplayInfoPtr display_info) {
  if (!display_info_)
    OnDisplayInfoChanged(std::move(display_info));
}

BrowserXRRuntime::~BrowserXRRuntime() = default;

void BrowserXRRuntime::OnDisplayInfoChanged(
    device::mojom::VRDisplayInfoPtr vr_device_info) {
  display_info_ = std::move(vr_device_info);
  for (XRDeviceImpl* device : renderer_device_connections_) {
    device->RuntimesChanged();
  }
}

void BrowserXRRuntime::StopImmersiveSession() {
  if (immersive_session_controller_) {
    immersive_session_controller_ = nullptr;
    presenting_renderer_device_ = nullptr;

    for (BrowserXRRuntimeObserver& observer : observers_) {
      observer.SetWebXRWebContents(nullptr);
    }
  }
}

void BrowserXRRuntime::OnExitPresent() {
  if (presenting_renderer_device_) {
    presenting_renderer_device_->OnExitPresent();
    presenting_renderer_device_ = nullptr;
  }
}

void BrowserXRRuntime::OnDeviceActivated(
    device::mojom::VRDisplayEventReason reason,
    base::OnceCallback<void(bool)> on_handled) {
  if (listening_for_activation_renderer_device_) {
    listening_for_activation_renderer_device_->OnActivate(
        reason, std::move(on_handled));
  } else {
    std::move(on_handled).Run(true /* will_not_present */);
  }
}

void BrowserXRRuntime::OnDeviceIdle(
    device::mojom::VRDisplayEventReason reason) {
  for (XRDeviceImpl* device : renderer_device_connections_) {
    device->OnDeactivate(reason);
  }
}

void BrowserXRRuntime::OnInitialized() {
  for (auto& callback : pending_initialization_callbacks_) {
    std::move(callback).Run(display_info_.Clone());
  }
  pending_initialization_callbacks_.clear();
}

void BrowserXRRuntime::OnRendererDeviceAdded(XRDeviceImpl* device) {
  renderer_device_connections_.insert(device);
}

void BrowserXRRuntime::OnRendererDeviceRemoved(XRDeviceImpl* device) {
  DCHECK(device);
  renderer_device_connections_.erase(device);
  if (device == presenting_renderer_device_) {
    ExitPresent(device);
    DCHECK(presenting_renderer_device_ == nullptr);
  }
  if (device == listening_for_activation_renderer_device_) {
    // Not listening for activation.
    listening_for_activation_renderer_device_ = nullptr;
    runtime_->SetListeningForActivate(false);
  }
}

void BrowserXRRuntime::ExitPresent(XRDeviceImpl* device) {
  if (device == presenting_renderer_device_) {
    StopImmersiveSession();
  }
}

void BrowserXRRuntime::RequestSession(
    XRDeviceImpl* device,
    const device::mojom::XRRuntimeSessionOptionsPtr& options,
    device::mojom::XRDevice::RequestSessionCallback callback) {
  // base::Unretained is safe because we won't be called back after runtime_ is
  // destroyed.
  runtime_->RequestSession(
      options->Clone(),
      base::BindOnce(&BrowserXRRuntime::OnRequestSessionResult,
                     base::Unretained(this), device->GetWeakPtr(),
                     options->Clone(), std::move(callback)));
}

void BrowserXRRuntime::OnRequestSessionResult(
    base::WeakPtr<XRDeviceImpl> device,
    device::mojom::XRRuntimeSessionOptionsPtr options,
    device::mojom::XRDevice::RequestSessionCallback callback,
    device::mojom::XRSessionPtr session,
    device::mojom::XRSessionControllerPtr immersive_session_controller) {
  if (session && device) {
    if (options->immersive) {
      presenting_renderer_device_ = device.get();
      immersive_session_controller_ = std::move(immersive_session_controller);
      immersive_session_controller_.set_connection_error_handler(base::BindOnce(
          &BrowserXRRuntime::OnImmersiveSessionError, base::Unretained(this)));

      // Notify observers that we have started presentation.
      content::WebContents* web_contents = device->GetWebContents();
      for (BrowserXRRuntimeObserver& observer : observers_) {
        observer.SetWebXRWebContents(web_contents);
      }
    }

    std::move(callback).Run(std::move(session));
  } else {
    std::move(callback).Run(nullptr);
    if (session) {
      // The device has been removed, but we still got a session, so make
      // sure to clean up this weird state.
      immersive_session_controller_ = std::move(immersive_session_controller);
      StopImmersiveSession();
    }
  }
}

void BrowserXRRuntime::OnImmersiveSessionError() {
  StopImmersiveSession();
}

void BrowserXRRuntime::UpdateListeningForActivate(XRDeviceImpl* device) {
  if (device->ListeningForActivate() && device->InFocusedFrame()) {
    bool was_listening = !!listening_for_activation_renderer_device_;
    listening_for_activation_renderer_device_ = device;
    if (!was_listening)
      OnListeningForActivate(true);
  } else if (listening_for_activation_renderer_device_ == device) {
    listening_for_activation_renderer_device_ = nullptr;
    OnListeningForActivate(false);
  }
}

void BrowserXRRuntime::InitializeAndGetDisplayInfo(
    device::mojom::XRDevice::GetImmersiveVRDisplayInfoCallback callback) {
  device::mojom::VRDisplayInfoPtr device_info = GetVRDisplayInfo();
  if (device_info) {
    std::move(callback).Run(std::move(device_info));
    return;
  }

  pending_initialization_callbacks_.push_back(std::move(callback));
  runtime_->EnsureInitialized(
      base::BindOnce(&BrowserXRRuntime::OnInitialized, base::Unretained(this)));
}

void BrowserXRRuntime::OnListeningForActivate(bool is_listening) {
  runtime_->SetListeningForActivate(is_listening);
}

}  // namespace vr
