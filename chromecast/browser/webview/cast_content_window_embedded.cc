// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webview/cast_content_window_embedded.h"

#include <string>

#include "base/check.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "chromecast/browser/cast_web_contents.h"
#include "chromecast/graphics/cast_window_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"

namespace chromecast {

namespace {
constexpr char kKeyAppId[] = "appId";
constexpr char kKeyAppSessionId[] = "appSessionId";
constexpr char kKeyRemoteControlModeEnabled[] = "remoteControlModeEnabled";
}  // namespace

CastContentWindowEmbedded::CastContentWindowEmbedded(
    mojom::CastWebViewParamsPtr params,
    CastWindowEmbedder* cast_window_embedder,
    bool force_720p_resolution)
    : CastContentWindow(std::move(params)),
      cast_window_embedder_(cast_window_embedder),
      force_720p_resolution_(force_720p_resolution) {
  DCHECK(cast_window_embedder_);

  cast_window_embedder_->AddEmbeddedWindow(this);
  window_id_ = cast_window_embedder_->GenerateWindowId();
}

void CastContentWindowEmbedded::SendWindowRequest(
    CastWindowEmbedder::WindowRequestType request_type) {
  cast_window_embedder_->OnWindowRequest(request_type,
                                         PopulateCastWindowProperties());
}

CastContentWindowEmbedded::~CastContentWindowEmbedded() {
  if (window_) {
    window_->RemoveObserver(this);
    window_ = nullptr;
  }
  if (cast_window_embedder_) {
    cast_window_embedder_->RemoveEmbeddedWindow(this);
  }

  SendWindowRequest(CastWindowEmbedder::WindowRequestType::CLOSE_WINDOW);
}

void CastContentWindowEmbedded::CreateWindow(
    ::chromecast::mojom::ZOrder z_order,
    VisibilityPriority visibility_priority) {
  if (!WebContents()) {
    LOG(ERROR) << "cast_web_contents is null";
    return;
  }
  Observe(WebContents());
  window_ = WebContents()->GetNativeView();
  visibility_priority_ = visibility_priority;
  if (!window_->HasObserver(this)) {
    window_->AddObserver(this);
  }
  if (!WebContents()->IsLoading()) {
    MaybeSendOpenWindowRequest();
  }
  WebContents()->Focus();
}

void CastContentWindowEmbedded::GrantScreenAccess() {
  has_screen_access_ = true;
  if (!open_window_sent_ && window_)
    MaybeSendOpenWindowRequest();
  else if (open_window_sent_)
    RequestFocus();
}

void CastContentWindowEmbedded::RevokeScreenAccess() {
  has_screen_access_ = false;
  ReleaseFocus();
}

void CastContentWindowEmbedded::EnableTouchInput(bool enabled) {}

void CastContentWindowEmbedded::RequestVisibility(
    VisibilityPriority visibility_priority) {
  visibility_priority_ = visibility_priority;

  // Since STICKY is sent to the embedder window manager when the app requests
  // HIDDEN, the app must be removed from focus before new visibility properties
  // are sent.
  if (visibility_priority == VisibilityPriority::HIDDEN ||
      visibility_priority == VisibilityPriority::HIDDEN_STICKY) {
    ReleaseFocus();
    SendCastWindowProperties();
  } else {
    SendCastWindowProperties();
    RequestFocus();
  }
}

void CastContentWindowEmbedded::SetActivityContext(
    base::Value activity_context) {
  activity_context_ = activity_context.Clone();

  auto* found_app_id = activity_context.FindKey(kKeyAppId);
  if (found_app_id) {
    app_id_ = found_app_id->GetString();
  } else {
    LOG(ERROR) << "App ID not found";
  }

  auto* found_remote_control =
      activity_context.FindKey(kKeyRemoteControlModeEnabled);
  if (found_remote_control) {
    is_remote_control_ = found_remote_control->GetBool();
  } else {
    LOG(ERROR) << "Is remote control not found";
  }

  auto* found_session_id = activity_context.FindKey(kKeyAppSessionId);
  if (found_session_id) {
    session_id_ = found_session_id->GetString();
  } else {
    LOG(ERROR) << "Session ID not found";
  }
}

void CastContentWindowEmbedded::SetHostContext(base::Value host_context) {
  host_context_ = host_context.Clone();
}

void CastContentWindowEmbedded::NotifyVisibilityChange(
    VisibilityType visibility_type) {
  for (auto& observer : observers_) {
    observer->OnVisibilityChange(visibility_type);
  }
}

void CastContentWindowEmbedded::RequestMoveOut() {}

void CastContentWindowEmbedded::OnWindowDestroyed(aura::Window* window) {
  window_ = nullptr;
  SendWindowRequest(CastWindowEmbedder::WindowRequestType::CLOSE_WINDOW);
}

void CastContentWindowEmbedded::OnEmbedderWindowEvent(
    const CastWindowEmbedder::EmbedderWindowEvent& request) {
  if (window_id_ != request.window_id)
    return;

  if (request.navigation && request.navigation.value() ==
                                CastWindowEmbedder::NavigationType::GO_BACK) {
    if (gesture_router()->CanHandleGesture(GestureType::GO_BACK)) {
      gesture_router()->ConsumeGesture(
          GestureType::GO_BACK,
          base::BindOnce(&CastContentWindowEmbedded::ConsumeGestureCompleted,
                         base::Unretained(this)));
    } else {
      cast_window_embedder_->GenerateAndSendNavigationHandleResult(
          window_id_, session_id_, false /* handled  */,
          CastWindowEmbedder::NavigationType::GO_BACK);
    }
    return;
  }

  if (request.visibility_changed) {
    switch (request.visibility_changed.value()) {
      case CastWindowEmbedder::VisibilityChange::UNKNOWN:
        NotifyVisibilityChange(VisibilityType::UNKNOWN);
        break;
      case CastWindowEmbedder::VisibilityChange::NOT_VISIBLE:
        NotifyVisibilityChange(VisibilityType::HIDDEN);
        break;
      case CastWindowEmbedder::VisibilityChange::FULL_SCREEN:
        NotifyVisibilityChange(VisibilityType::FULL_SCREEN);
        break;
      case CastWindowEmbedder::VisibilityChange::OBSCURED:
        NotifyVisibilityChange(VisibilityType::TRANSIENTLY_HIDDEN);
        break;
      case CastWindowEmbedder::VisibilityChange::INTERRUPTION:
        NotifyVisibilityChange(VisibilityType::PARTIAL_OUT);
        break;
      case CastWindowEmbedder::VisibilityChange::INTERRUPTED:
        NotifyVisibilityChange(VisibilityType::FULL_SCREEN);
        break;
    }
    return;
  }

  if (request.back_gesture_progress_event) {
    if (gesture_router()->CanHandleGesture(GestureType::GO_BACK)) {
      gesture_router()->GestureProgress(
          GestureType::GO_BACK,
          gfx::Point(request.back_gesture_progress_event.value().x,
                     request.back_gesture_progress_event.value().y));
    }
    return;
  }

  if (request.back_gesture_cancel_event) {
    if (gesture_router()->CanHandleGesture(GestureType::GO_BACK))
      gesture_router()->CancelGesture(GestureType::GO_BACK);
    return;
  }
}

void CastContentWindowEmbedded::ConsumeGestureCompleted(bool handled) {
  cast_window_embedder_->GenerateAndSendNavigationHandleResult(
      window_id_, session_id_, handled,
      CastWindowEmbedder::NavigationType::GO_BACK);
}

int CastContentWindowEmbedded::GetWindowId() {
  return window_id_;
}

std::string CastContentWindowEmbedded::GetAppId() {
  return app_id_;
}

content::WebContents* CastContentWindowEmbedded::GetWebContents() {
  DCHECK(cast_web_contents());
  return WebContents();
}

CastWebContents* CastContentWindowEmbedded::GetCastWebContents() {
  return cast_web_contents_;
}

void CastContentWindowEmbedded::DispatchState() {
  SendOpenWindowRequest();
  RequestFocus();
}

void CastContentWindowEmbedded::SendAppContext(const std::string& context) {
  auto cast_window_properties = PopulateCastWindowProperties();
  cast_window_properties.app_context = context;
  cast_window_embedder_->OnWindowRequest(
      CastWindowEmbedder::WindowRequestType::SET_PROPERTIES,
      cast_window_properties);
}

void CastContentWindowEmbedded::Stop() {
  if (cast_web_contents_)
    cast_web_contents_->Stop(net::ERR_FAILED);
}

void CastContentWindowEmbedded::SetCanGoBack(bool can_go_back) {
  can_go_back_ = can_go_back;
  if (open_window_sent_)
    SendCastWindowProperties();
}

void CastContentWindowEmbedded::RegisterBackGestureRouter(
    ::chromecast::BackGestureRouter* back_gesture_router) {
  back_gesture_router->SetBackGestureDelegate(this);
}

CastWindowEmbedder::CastWindowProperties
CastContentWindowEmbedded::PopulateCastWindowProperties() {
  CastWindowEmbedder::CastWindowProperties window_properties;
  window_properties.window_id = window_id_;
  window_properties.session_id = session_id_;
  window_properties.app_id = app_id_;
  window_properties.is_system_setup_window = false;
  window_properties.is_touch_enabled = params_->enable_touch_input;
  window_properties.is_remote_control = is_remote_control_;
  window_properties.visibility_priority = visibility_priority_;
  window_properties.force_720p_resolution = force_720p_resolution_;
  window_properties.supports_go_back_inside = can_go_back_;
  window_properties.host_context = host_context_.Clone();
  return window_properties;
}

void CastContentWindowEmbedded::ReleaseFocus() {
  if (!window_) {
    LOG(WARNING) << "window_ is null";
    return;
  }
  SendWindowRequest(CastWindowEmbedder::WindowRequestType::RELEASE_FOCUS);

  // Because rendering a larger window may require more system resources,
  // resize the window to one pixel while hidden.
  LOG(INFO) << "Resizing window to 1x1 pixel while hidden";
  window_->SetBounds(gfx::Rect(1, 1));
}

void CastContentWindowEmbedded::RequestFocus() {
  if (!has_screen_access_)
    return;

  if (!window_) {
    LOG(WARNING) << "window_ is null";
    return;
  }

  SendWindowRequest(CastWindowEmbedder::WindowRequestType::REQUEST_FOCUS);
}

void CastContentWindowEmbedded::MaybeSendOpenWindowRequest() {
  if (open_window_sent_ || !has_screen_access_)
    return;

  SendOpenWindowRequest();
  RequestFocus();
  open_window_sent_ = true;
}

void CastContentWindowEmbedded::SendCastWindowProperties() {
  SendWindowRequest(CastWindowEmbedder::WindowRequestType::SET_PROPERTIES);
}

void CastContentWindowEmbedded::SendOpenWindowRequest() {
  SendWindowRequest(CastWindowEmbedder::WindowRequestType::OPEN_WINDOW);
}

void CastContentWindowEmbedded::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  MaybeSendOpenWindowRequest();
}

void CastContentWindowEmbedded::DidFirstVisuallyNonEmptyPaint() {
  MaybeSendOpenWindowRequest();
}

void CastContentWindowEmbedded::WebContentsDestroyed() {
  cast_web_contents_ = nullptr;
}

}  // namespace chromecast
