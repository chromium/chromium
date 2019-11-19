// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webview/web_content_controller.h"

#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "chromecast/base/version.h"
#include "chromecast/browser/webview/proto/webview.pb.h"
#include "chromecast/browser/webview/webview_layout_manager.h"
#include "chromecast/browser/webview/webview_navigation_throttle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/web_preferences.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/events/event.h"

namespace chromecast {

WebContentController::WebContentController(Client* client) : client_(client) {
  js_channels_ = std::make_unique<WebContentJsChannels>(client_);
  JsClientInstance::AddObserver(this);
}

WebContentController::~WebContentController() {
  JsClientInstance::RemoveObserver(this);
  if (surface_) {
    surface_->RemoveSurfaceObserver(this);
    surface_->SetEmbeddedSurfaceId(base::RepeatingCallback<viz::SurfaceId()>());
  }
}

void WebContentController::ProcessRequest(
    const webview::WebviewRequest& request) {
  content::WebContents* contents = GetWebContents();
  switch (request.type_case()) {
    case webview::WebviewRequest::kInput:
      ProcessInputEvent(request.input());
      break;

    case webview::WebviewRequest::kEvaluateJavascript:
      if (request.has_evaluate_javascript()) {
        HandleEvaluateJavascript(request.id(), request.evaluate_javascript());
      } else {
        client_->OnError("evaluate_javascript() not supplied");
      }
      break;

    case webview::WebviewRequest::kAddJavascriptChannels:
      if (request.has_add_javascript_channels()) {
        HandleAddJavascriptChannels(request.add_javascript_channels());
      } else {
        client_->OnError("add_javascript_channels() not supplied");
      }
      break;

    case webview::WebviewRequest::kRemoveJavascriptChannels:
      if (request.has_remove_javascript_channels()) {
        HandleRemoveJavascriptChannels(request.remove_javascript_channels());
      } else {
        client_->OnError("remove_javascript_channels() not supplied");
      }
      break;

    case webview::WebviewRequest::kGetCurrentUrl:
      HandleGetCurrentUrl(request.id());
      break;

    case webview::WebviewRequest::kCanGoBack:
      HandleCanGoBack(request.id());
      break;

    case webview::WebviewRequest::kCanGoForward:
      HandleCanGoForward(request.id());
      break;

    case webview::WebviewRequest::kGoBack:
      contents->GetController().GoBack();
      break;

    case webview::WebviewRequest::kGoForward:
      contents->GetController().GoForward();
      break;

    case webview::WebviewRequest::kReload:
      // TODO(dnicoara): Are the default parameters correct?
      contents->GetController().Reload(content::ReloadType::NORMAL,
                                       /*check_for_repost=*/true);
      break;

    case webview::WebviewRequest::kClearCache:
      HandleClearCache();
      break;

    case webview::WebviewRequest::kUpdateSettings:
      if (request.has_update_settings()) {
        HandleUpdateSettings(request.update_settings());
      } else {
        client_->OnError("update_settings() not supplied");
      }
      break;

    case webview::WebviewRequest::kGetTitle:
      HandleGetTitle(request.id());
      break;

    case webview::WebviewRequest::kSetAutoMediaPlaybackPolicy:
      if (request.has_set_auto_media_playback_policy()) {
        HandleSetAutoMediaPlaybackPolicy(
            request.set_auto_media_playback_policy());
      } else {
        client_->OnError("set_auto_media_playback_policy() not supplied");
      }
      break;

    case webview::WebviewRequest::kResize:
      if (request.has_resize()) {
        GetWebContents()->GetNativeView()->SetBounds(
            gfx::Rect(request.resize().width(), request.resize().height()));
      } else {
        client_->OnError("resize() not supplied");
      }
      break;

    default:
      client_->OnError("Unknown request code");
      break;
  }
}

void WebContentController::AttachTo(aura::Window* window, int window_id) {
  content::WebContents* contents = GetWebContents();
  auto* contents_window = contents->GetNativeView();
  window->SetLayoutManager(new WebviewLayoutManager(window));
  contents_window->set_id(window_id);
  contents_window->SetBounds(gfx::Rect(window->bounds().size()));
  // The aura window is hidden to avoid being shown via the usual layer method,
  // instead it is shows via a SurfaceDrawQuad by exo.
  contents_window->Hide();
  window->AddChild(contents_window);

  exo::Surface* surface = exo::Surface::AsSurface(window);
  CHECK(surface) << "Attaching Webview to non-EXO surface window";
  CHECK(!surface_) << "Attaching already attached WebView";

  surface_ = surface;
  surface_->AddSurfaceObserver(this);

  // Unretained is safe because we unset this in the destructor.
  surface_->SetEmbeddedSurfaceId(
      base::Bind(&WebContentController::GetSurfaceId, base::Unretained(this)));
}

void WebContentController::ProcessInputEvent(const webview::InputEvent& ev) {
  content::WebContents* contents = GetWebContents();
  // Ensure this web contents has focus before sending it input.
  if (!contents->GetNativeView()->HasFocus())
    contents->GetNativeView()->Focus();

  ui::EventHandler* handler =
      contents->GetRenderWidgetHostView()->GetNativeView()->delegate();
  ui::EventType type = static_cast<ui::EventType>(ev.event_type());
  switch (type) {
    case ui::ET_TOUCH_RELEASED:
    case ui::ET_TOUCH_PRESSED:
    case ui::ET_TOUCH_MOVED:
    case ui::ET_TOUCH_CANCELLED:
      if (ev.has_touch()) {
        auto& touch = ev.touch();
        ui::TouchEvent evt(
            type, gfx::PointF(touch.x(), touch.y()),
            gfx::PointF(touch.root_x(), touch.root_y()),
            base::TimeTicks() +
                base::TimeDelta::FromMicroseconds(ev.timestamp()),
            ui::PointerDetails(
                static_cast<ui::EventPointerType>(touch.pointer_type()),
                static_cast<ui::PointerId>(touch.pointer_id()),
                touch.radius_x(), touch.radius_y(), touch.force(),
                touch.twist(), touch.tilt_x(), touch.tilt_y(),
                touch.tangential_pressure()),
            ev.flags());

        ui::TouchEvent root_relative_event(evt);
        root_relative_event.set_location_f(evt.root_location_f());

        // GestureRecognizerImpl makes several APIs private so cast it to the
        // interface.
        ui::GestureRecognizer* recognizer = &gesture_recognizer_;

        // Run touches through the gesture recognition pipeline, web content
        // typically wants to process gesture events, not touch events.
        if (!recognizer->ProcessTouchEventPreDispatch(
                &root_relative_event, contents->GetNativeView())) {
          return;
        }

        handler->OnTouchEvent(&evt);

        // Normally this would be done when the renderer acknowledges the touch
        // event and using flags from the renderer, inside
        // RenderWidgetHostViewAura, but we don't have those so... fake it.
        auto list =
            recognizer->AckTouchEvent(evt.unique_event_id(), ui::ER_UNHANDLED,
                                      false, contents->GetNativeView());
        for (auto& e : list) {
          // Forward all gestures.
          handler->OnGestureEvent(e.get());
        }
      } else {
        client_->OnError("touch() not supplied for touch event");
      }
      break;
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_MOUSE_DRAGGED:
    case ui::ET_MOUSE_RELEASED:
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED:
    case ui::ET_MOUSEWHEEL:
    case ui::ET_MOUSE_CAPTURE_CHANGED:
      if (ev.has_mouse()) {
        auto& mouse = ev.mouse();
        ui::MouseEvent evt(
            type, gfx::PointF(mouse.x(), mouse.y()),
            gfx::PointF(mouse.root_x(), mouse.root_y()),
            base::TimeTicks() +
                base::TimeDelta::FromMicroseconds(ev.timestamp()),
            ev.flags(), mouse.changed_button_flags());
        handler->OnMouseEvent(&evt);
      } else {
        client_->OnError("mouse() not supplied for mouse event");
      }
      break;
    default:
      break;
  }
}

void WebContentController::JavascriptCallback(int64_t id, base::Value result) {
  std::string json;
  base::JSONWriter::Write(result, &json);
  std::unique_ptr<webview::WebviewResponse> response =
      std::make_unique<webview::WebviewResponse>();
  response->set_id(id);
  response->mutable_evaluate_javascript()->set_json(json);
  client_->EnqueueSend(std::move(response));
}

void WebContentController::HandleEvaluateJavascript(
    int64_t id,
    const webview::EvaluateJavascriptRequest& request) {
  GetWebContents()->GetMainFrame()->ExecuteJavaScript(
      base::UTF8ToUTF16(request.javascript_blob()),
      base::BindOnce(&WebContentController::JavascriptCallback,
                     weak_ptr_factory_.GetWeakPtr(), id));
}

void WebContentController::HandleAddJavascriptChannels(
    const webview::AddJavascriptChannelsRequest& request) {
  for (auto& channel : request.channels()) {
    current_javascript_channel_set_.insert(channel);
    for (auto* frame : current_render_frame_set_) {
      ChannelModified(frame, channel, true);
    }
  }
}

void WebContentController::HandleRemoveJavascriptChannels(
    const webview::RemoveJavascriptChannelsRequest& request) {
  for (auto& channel : request.channels()) {
    current_javascript_channel_set_.erase(channel);
    for (auto* frame : current_render_frame_set_) {
      ChannelModified(frame, channel, false);
    }
  }
}

void WebContentController::HandleGetCurrentUrl(int64_t id) {
  std::unique_ptr<webview::WebviewResponse> response =
      std::make_unique<webview::WebviewResponse>();

  response->set_id(id);
  response->mutable_get_current_url()->set_url(
      GetWebContents()->GetURL().spec());
  client_->EnqueueSend(std::move(response));
}

void WebContentController::HandleCanGoBack(int64_t id) {
  std::unique_ptr<webview::WebviewResponse> response =
      std::make_unique<webview::WebviewResponse>();

  response->set_id(id);
  response->mutable_can_go_back()->set_can_go_back(
      GetWebContents()->GetController().CanGoBack());
  client_->EnqueueSend(std::move(response));
}

void WebContentController::HandleCanGoForward(int64_t id) {
  std::unique_ptr<webview::WebviewResponse> response =
      std::make_unique<webview::WebviewResponse>();

  response->set_id(id);
  response->mutable_can_go_forward()->set_can_go_forward(
      GetWebContents()->GetController().CanGoForward());
  client_->EnqueueSend(std::move(response));
}

void WebContentController::HandleClearCache() {
  // TODO(dnicoara): See if there is a generic way to inform the renderer to
  // clear cache.
  // Android has a specific renderer message for this:
  // https://cs.chromium.org/chromium/src/android_webview/common/render_view_messages.h?rcl=65107121555167a3db39de5633c3297f7e861315&l=44

  // Remove disk cache.
  content::BrowsingDataRemover* remover =
      content::BrowserContext::GetBrowsingDataRemover(
          GetWebContents()->GetBrowserContext());
  remover->Remove(base::Time(), base::Time::Max(),
                  content::BrowsingDataRemover::DATA_TYPE_CACHE,
                  content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
                      content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB);
}

void WebContentController::HandleGetTitle(int64_t id) {
  std::unique_ptr<webview::WebviewResponse> response =
      std::make_unique<webview::WebviewResponse>();

  response->set_id(id);
  response->mutable_get_title()->set_title(
      base::UTF16ToUTF8(GetWebContents()->GetTitle()));
  client_->EnqueueSend(std::move(response));
}

void WebContentController::HandleUpdateSettings(
    const webview::UpdateSettingsRequest& request) {
  content::WebContents* contents = GetWebContents();
  content::WebPreferences prefs =
      contents->GetRenderViewHost()->GetWebkitPreferences();
  prefs.javascript_enabled = request.javascript_enabled();
  contents->GetRenderViewHost()->UpdateWebkitPreferences(prefs);

  has_navigation_delegate_ = request.has_navigation_delegate();

  // Given that cast_shell enables devtools unconditionally there isn't
  // anything that needs to be done for |request.debugging_enabled()|. Though,
  // as a note, remote debugging is always on.

  if (request.has_user_agent() &&
      request.user_agent().type_case() == webview::UserAgent::kValue) {
    contents->SetUserAgentOverride(request.user_agent().value(), true);
  }
}

void WebContentController::HandleSetAutoMediaPlaybackPolicy(
    const webview::SetAutoMediaPlaybackPolicyRequest& request) {
  content::WebContents* contents = GetWebContents();
  content::WebPreferences prefs =
      contents->GetRenderViewHost()->GetWebkitPreferences();
  prefs.autoplay_policy = request.require_user_gesture()
                              ? content::AutoplayPolicy::kUserGestureRequired
                              : content::AutoplayPolicy::kNoUserGestureRequired;
  contents->GetRenderViewHost()->UpdateWebkitPreferences(prefs);
}

viz::SurfaceId WebContentController::GetSurfaceId() {
  auto* rwhv = GetWebContents()->GetRenderWidgetHostView();
  if (!rwhv)
    return viz::SurfaceId();
  auto frame_sink_id = rwhv->GetRenderWidgetHost()->GetFrameSinkId();
  auto local_surface_id =
      rwhv->GetNativeView()->GetLocalSurfaceIdAllocation().local_surface_id();
  return viz::SurfaceId(frame_sink_id, local_surface_id);
}

void WebContentController::OnSurfaceDestroying(exo::Surface* surface) {
  DCHECK_EQ(surface, surface_);
  surface->RemoveSurfaceObserver(this);
  surface_ = nullptr;
}

void WebContentController::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  current_render_frame_set_.insert(render_frame_host);
  auto* instance =
      JsClientInstance::Find(render_frame_host->GetProcess()->GetID(),
                             render_frame_host->GetRoutingID());
  // If the instance doesn't exist yet the JsClientInstance observer will see
  // it later on.
  if (instance)
    SendInitialChannelSet(instance);
}

void WebContentController::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  current_render_frame_set_.erase(render_frame_host);
}

void WebContentController::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  // The surface ID may have changed, so trigger a new commit to re-issue the
  // draw quad.
  if (surface_)
    surface_->Commit();
}

void WebContentController::OnJsClientInstanceRegistered(
    int process_id,
    int routing_id,
    JsClientInstance* instance) {
  if (current_render_frame_set_.find(content::RenderFrameHost::FromID(
          process_id, routing_id)) != current_render_frame_set_.end()) {
    // If the frame exists in the set then it cannot have been handled by
    // RenderFrameCreated.
    SendInitialChannelSet(instance);
  }
}

void WebContentController::ChannelModified(content::RenderFrameHost* frame,
                                           const std::string& channel,
                                           bool added) {
  auto* instance = JsClientInstance::Find(frame->GetProcess()->GetID(),
                                          frame->GetRoutingID());
  if (instance) {
    if (added) {
      instance->AddChannel(channel, GetJsChannelCallback());
    } else {
      instance->RemoveChannel(channel);
    }
  } else {
    LOG(WARNING) << "Cannot change channel " << channel << " for "
                 << frame->GetLastCommittedURL().possibly_invalid_spec();
  }
}

JsChannelCallback WebContentController::GetJsChannelCallback() {
  return base::BindRepeating(&WebContentJsChannels::SendMessage,
                             js_channels_->AsWeakPtr());
}

void WebContentController::SendInitialChannelSet(JsClientInstance* instance) {
  JsChannelCallback callback = GetJsChannelCallback();
  for (auto& channel : current_javascript_channel_set_)
    instance->AddChannel(channel, callback);
}

WebContentJsChannels::WebContentJsChannels(WebContentController::Client* client)
    : client_(client) {}

WebContentJsChannels::~WebContentJsChannels() = default;

void WebContentJsChannels::SendMessage(const std::string& channel,
                                       const std::string& message) {
  std::unique_ptr<webview::WebviewResponse> response =
      std::make_unique<webview::WebviewResponse>();
  auto* js_message = response->mutable_javascript_channel_message();
  js_message->set_channel(channel);
  js_message->set_message(message);
  client_->EnqueueSend(std::move(response));
}

}  // namespace chromecast
