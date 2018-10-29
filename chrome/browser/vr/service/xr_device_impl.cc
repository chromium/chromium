// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/service/xr_device_impl.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "chrome/browser/vr/metrics/session_metrics_helper.h"
#include "chrome/browser/vr/mode.h"
#include "chrome/browser/vr/service/browser_xr_runtime.h"
#include "chrome/browser/vr/service/xr_runtime_manager.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/origin_util.h"
#include "device/vr/vr_display_impl.h"

namespace vr {

namespace {

// TODO(mthiesse): When we unship WebVR 1.1, set this to false.
static constexpr bool kAllowHTTPWebVRWithFlag = true;

bool IsSecureContext(content::RenderFrameHost* host) {
  if (!host)
    return false;
  while (host != nullptr) {
    if (!content::IsOriginSecure(host->GetLastCommittedURL()))
      return false;
    host = host->GetParent();
  }
  return true;
}

device::mojom::XRRuntimeSessionOptionsPtr GetRuntimeOptions(
    device::mojom::XRSessionOptions* options) {
  device::mojom::XRRuntimeSessionOptionsPtr runtime_options =
      device::mojom::XRRuntimeSessionOptions::New();
  runtime_options->immersive = options->immersive;
  runtime_options->has_user_activation = options->has_user_activation;
  runtime_options->provide_passthrough_camera =
      options->provide_passthrough_camera;
  runtime_options->use_legacy_webvr_render_path =
      options->use_legacy_webvr_render_path;
  return runtime_options;
}

}  // namespace

XRDeviceImpl::XRDeviceImpl(content::RenderFrameHost* render_frame_host,
                           device::mojom::XRDeviceRequest request)
    :  // TODO(https://crbug.com/846392): render_frame_host can be null because
       // of a test, not because a XRDeviceImpl can be created without it.
      in_focused_frame_(
          render_frame_host ? render_frame_host->GetView()->HasFocus() : false),
      render_frame_host_(render_frame_host),
      binding_(this),
      weak_ptr_factory_(this) {
  binding_.Bind(std::move(request));
}

void XRDeviceImpl::OnNonImmersiveSessionCreated(
    device::mojom::XRDevice::RequestSessionCallback callback,
    device::mojom::XRSessionPtr session,
    device::mojom::XRSessionControllerPtr controller) {
  if (!session) {
    std::move(callback).Run(nullptr);
    return;
  }

  // Start giving out magic window data if we are focused.
  controller->SetFrameDataRestricted(!in_focused_frame_);

  magic_window_controllers_.AddPtr(std::move(controller));

  OnSessionCreated(std::move(callback), std::move(session));
}

void XRDeviceImpl::OnSessionCreated(
    device::mojom::XRDevice::RequestSessionCallback callback,
    device::mojom::XRSessionPtr session) {
  if (!session) {
    std::move(callback).Run(nullptr);
    return;
  }

  device::mojom::XRSessionClientPtr client;
  session->client_request = mojo::MakeRequest(&client);

  session_clients_.AddPtr(std::move(client));

  std::move(callback).Run(std::move(session));
}

XRDeviceImpl::~XRDeviceImpl() {
  XRRuntimeManager::GetInstance()->OnRendererDeviceRemoved(this);
}

void XRDeviceImpl::RequestSession(
    device::mojom::XRSessionOptionsPtr options,
    bool triggered_by_displayactive,
    device::mojom::XRDevice::RequestSessionCallback callback) {
  DCHECK(options);

  // Check that the request satisifies secure context requirements.
  if (!IsSecureContextRequirementSatisfied()) {
    std::move(callback).Run(nullptr);
    return;
  }

  // Check that the request is coming from a focused page if required.
  if (!in_focused_frame_ && options->immersive) {
    std::move(callback).Run(nullptr);
    return;
  }

  if (XRRuntimeManager::GetInstance()->IsOtherDevicePresenting(this)) {
    // Can't create sessions while an immersive session exists.
    std::move(callback).Run(nullptr);
    return;
  }

  // Get the runtime we'll be creating a session with.
  BrowserXRRuntime* runtime =
      XRRuntimeManager::GetInstance()->GetRuntimeForOptions(options.get());
  if (!runtime) {
    std::move(callback).Run(nullptr);
    return;
  }

  auto runtime_options = GetRuntimeOptions(options.get());
  runtime_options->render_process_id =
      render_frame_host_ ? render_frame_host_->GetProcess()->GetID() : -1;
  runtime_options->render_frame_id =
      render_frame_host_ ? render_frame_host_->GetRoutingID() : -1;

  if (runtime_options->immersive) {
    if (!triggered_by_displayactive) {
      ReportRequestPresent();
    }

    base::OnceCallback<void(device::mojom::XRSessionPtr)> immersive_callback =
        base::BindOnce(&XRDeviceImpl::OnSessionCreated,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback));
    runtime->RequestSession(this, std::move(runtime_options),
                            std::move(immersive_callback));
  } else {
    base::OnceCallback<void(device::mojom::XRSessionPtr,
                            device::mojom::XRSessionControllerPtr)>
        non_immersive_callback =
            base::BindOnce(&XRDeviceImpl::OnNonImmersiveSessionCreated,
                           weak_ptr_factory_.GetWeakPtr(), std::move(callback));
    runtime->GetRuntime()->RequestSession(std::move(runtime_options),
                                          std::move(non_immersive_callback));
  }
}

void XRDeviceImpl::SupportsSession(
    device::mojom::XRSessionOptionsPtr options,
    device::mojom::XRDevice::SupportsSessionCallback callback) {
  XRRuntimeManager::GetInstance()->SupportsSession(std::move(options),
                                                   std::move(callback));
}

void XRDeviceImpl::ReportRequestPresent() {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host_);
  SessionMetricsHelper* metrics_helper =
      SessionMetricsHelper::FromWebContents(web_contents);
  if (!metrics_helper) {
    // This will only happen if we are not already in VR, set start params
    // accordingly.
    metrics_helper = SessionMetricsHelper::CreateForWebContents(
        web_contents, Mode::kNoVr, false);
  }
  metrics_helper->ReportRequestPresent();
}

void XRDeviceImpl::ExitPresent() {
  BrowserXRRuntime* immersive_runtime =
      XRRuntimeManager::GetInstance()->GetImmersiveRuntime();
  if (immersive_runtime)
    immersive_runtime->ExitPresent(this);
}

void XRDeviceImpl::SetListeningForActivate(
    device::mojom::VRDisplayClientPtr client) {
  client_ = std::move(client);
  BrowserXRRuntime* immersive_runtime =
      XRRuntimeManager::GetInstance()->GetImmersiveRuntime();
  if (immersive_runtime && client_) {
    immersive_runtime->UpdateListeningForActivate(this);
  }
}

void XRDeviceImpl::GetImmersiveVRDisplayInfo(
    device::mojom::XRDevice::GetImmersiveVRDisplayInfoCallback callback) {
  BrowserXRRuntime* immersive_runtime =
      XRRuntimeManager::GetInstance()->GetImmersiveRuntime();
  if (!immersive_runtime) {
    std::move(callback).Run(nullptr);
    return;
  }

  immersive_runtime->InitializeAndGetDisplayInfo(std::move(callback));
}

void XRDeviceImpl::SetInFocusedFrame(bool in_focused_frame) {
  in_focused_frame_ = in_focused_frame;
  if (ListeningForActivate()) {
    BrowserXRRuntime* immersive_runtime =
        XRRuntimeManager::GetInstance()->GetImmersiveRuntime();
    if (immersive_runtime)
      immersive_runtime->UpdateListeningForActivate(this);
  }

  magic_window_controllers_.ForAllPtrs(
      [&in_focused_frame](device::mojom::XRSessionController* controller) {
        controller->SetFrameDataRestricted(!in_focused_frame);
      });
}

void XRDeviceImpl::RuntimesChanged() {
  device::mojom::VRDisplayInfoPtr display_info =
      XRRuntimeManager::GetInstance()->GetCurrentVRDisplayInfo(this);
  if (display_info) {
    session_clients_.ForAllPtrs(
        [&display_info](device::mojom::XRSessionClient* client) {
          client->OnChanged(display_info.Clone());
        });
  }
}

void XRDeviceImpl::OnExitPresent() {
  session_clients_.ForAllPtrs(
      [](device::mojom::XRSessionClient* client) { client->OnExitPresent(); });
}

// TODO(http://crbug.com/845283): We should store the state here and send blur
// messages to sessions that start blurred.
void XRDeviceImpl::OnBlur() {
  session_clients_.ForAllPtrs(
      [](device::mojom::XRSessionClient* client) { client->OnBlur(); });
}

void XRDeviceImpl::OnFocus() {
  session_clients_.ForAllPtrs(
      [](device::mojom::XRSessionClient* client) { client->OnFocus(); });
}

void XRDeviceImpl::OnActivate(device::mojom::VRDisplayEventReason reason,
                              base::OnceCallback<void(bool)> on_handled) {
  if (client_) {
    client_->OnActivate(reason, std::move(on_handled));
  }
}

void XRDeviceImpl::OnDeactivate(device::mojom::VRDisplayEventReason reason) {
  if (client_) {
    client_->OnDeactivate(reason);
  }
}

content::WebContents* XRDeviceImpl::GetWebContents() {
  if (render_frame_host_ != nullptr) {
    return content::WebContents::FromRenderFrameHost(render_frame_host_);
  }

  // We should only have a null render_frame_host_ for some unittests, for which
  // we don't actually expect to get here.
  NOTREACHED();
  return nullptr;
}

bool XRDeviceImpl::IsSecureContextRequirementSatisfied() {
  // We require secure connections unless both the webvr flag and the
  // http flag are enabled.
  bool requires_secure_context =
      !kAllowHTTPWebVRWithFlag ||
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableWebVR);
  if (!requires_secure_context)
    return true;
  return IsSecureContext(render_frame_host_);
}

}  // namespace vr
