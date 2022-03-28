// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webview/web_content_controller.h"

#include <utility>

#include "base/containers/cxx20_erase.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chromecast/base/version.h"
#include "chromecast/browser/cast_web_contents.h"
#include "chromecast/browser/webview/proto/webview.pb.h"
#include "chromecast/browser/webview/webview_input_method_observer.h"
#include "chromecast/browser/webview/webview_navigation_throttle.h"
#include "chromecast/common/user_agent.h"
#include "chromecast/graphics/cast_focus_client_aura.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/constants.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/insets.h"

namespace chromecast {

namespace {

base::TimeTicks TimeTicksFromTimestamp(int64_t timestamp) {
  return base::TimeTicks() + base::Microseconds(timestamp);
}

}  // namespace

WebContentController::WebviewWindowVisibilityObserver::
    WebviewWindowVisibilityObserver(aura::Window* window,
                                    WebContentController* controller)
    : window_(window), controller_(controller) {
  DCHECK(window_);
  DCHECK(controller_);
  window_->AddObserver(this);
}

void WebContentController::WebviewWindowVisibilityObserver::
    OnWindowVisibilityChanged(aura::Window* window, bool visible) {
  if (window == window_ && visible && window->CanFocus())
    controller_->OnVisible(window);
}

void WebContentController::WebviewWindowVisibilityObserver::OnWindowDestroyed(
    aura::Window* window) {
  if (window == window_)
    window_ = nullptr;
}

WebContentController::WebviewWindowVisibilityObserver::
    ~WebviewWindowVisibilityObserver() {
  if (window_)
    window_->RemoveObserver(this);
}

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
  if (!current_render_widget_set_.empty()) {
    // TODO(b/150955487): A WebContentController can be destructed without us
    // having received RenderViewDeleted notifications for all observed
    // RenderWidgetHosts, so we go through the current_render_widget_set_ to
    // remove the input event observers. It has sometimes been the case (perhaps
    // only on a renderer process crash; requires investigation) that an
    // observed RenderWidgetHost has disappeared without notification.
    // Therefore, it is not safe to call RemoveInputEventObserver on every
    // RenderWidgetHost that we started observing; we need to remove only
    // from currently live RenderWidgetHosts.
    std::unique_ptr<content::RenderWidgetHostIterator> widgets(
        content::RenderWidgetHost::GetRenderWidgetHosts());
    while (content::RenderWidgetHost* widget = widgets->GetNextHost()) {
      auto it = current_render_widget_set_.find(widget);
      if (it != current_render_widget_set_.end()) {
        widget->RemoveInputEventObserver(this);
      }
    }
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

    case webview::WebviewRequest::kClearCookies:
      HandleClearCookies(request.id());
      break;

    case webview::WebviewRequest::kGetTitle:
      HandleGetTitle(request.id());
      break;

    case webview::WebviewRequest::kResize:
      if (request.has_resize()) {
        HandleResize(
            gfx::Size(request.resize().width(), request.resize().height()));
      } else {
        client_->OnError("resize() not supplied");
      }
      break;

    case webview::WebviewRequest::kSetInsets:
      if (request.has_set_insets()) {
        HandleSetInsets(gfx::Insets::TLBR(
            request.set_insets().top(), request.set_insets().left(),
            request.set_insets().bottom(), request.set_insets().right()));
      } else {
        client_->OnError("set_insets() not supplied");
      }
      break;

    case webview::WebviewRequest::kGetUserAgent:
      HandleGetUserAgent(request.id());
      break;

    case webview::WebviewRequest::kFocus:
      contents->GetNativeView()->Focus();
      break;

    default:
      client_->OnError("Unknown request code");
      break;
  }
}

void WebContentController::AttachTo(aura::Window* window, int window_id) {
  // Register our observer on the window so we can act later once it
  // becomes visible.
  window_visibility_observer_ =
      std::make_unique<WebviewWindowVisibilityObserver>(window, this);

  content::WebContents* contents = GetWebContents();
  auto* contents_window = contents->GetNativeView();
  contents_window->SetId(window_id);
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
  surface_->SetEmbeddedSurfaceId(base::BindRepeating(
      &WebContentController::GetSurfaceId, base::Unretained(this)));
  HandleResize(contents_window->bounds().size());
}

void WebContentController::OnVisible(aura::Window* window) {
  // Acquire initial focus.
  auto* contents = GetWebContents();
  if (contents) {
    contents->SetInitialFocus();
  } else {
    LOG(WARNING)
        << "Webview unable to acquire initial focus due to missing webcontents";
  }

  // Register for IME events
  input_method_observer_ = std::make_unique<WebviewInputMethodObserver>(
      this, window->GetHost()->GetInputMethod());
}

void WebContentController::ProcessInputEvent(const webview::InputEvent& ev) {
  content::WebContents* contents = GetWebContents();
  DCHECK(contents);

  // Ensure this web contents has focus before sending it input.
  // Focus at this level is necessary, or else Blink will ignore
  // attempts to focus any elements in the contents.
  //
  // Via b/156123509: The aura::Window given by |contents->GetNativeView()|
  // is not suitable for this purpose, because it has no OnWindowFocused
  // observer. The |window| used here is the same one whose |delegate|
  // is the EventHandler for this input event.
  content::RenderWidgetHostView* rwhv = contents->GetRenderWidgetHostView();
  aura::Window* window = rwhv->GetNativeView();
  DCHECK(window == contents->GetContentNativeView());
  if (!window->CanFocus())
    return;
  if (!window->HasFocus())
    window->Focus();

  ui::EventHandler* handler = rwhv->GetNativeView()->delegate();
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
            base::TimeTicks() + base::Microseconds(ev.timestamp()),
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
        // This flag is set depending on the gestures recognized in the call
        // above, and needs to propagate with the forwarded event.
        evt.set_may_cause_scrolling(root_relative_event.may_cause_scrolling());

        if (type == ui::ET_TOUCH_PRESSED) {
          // Ensure that we are observing the RenderWidgetHost for this touch
          // sequence, even if we didn't get a WebContentsObserver notification
          // for its creation. (This is not the normal case, but can happen
          // e.g. when loading a page with the Fling interface.)
          RegisterRenderWidgetInputObserver(rwhv->GetRenderWidgetHost());
        }

        // Record touch event information to match against acks.
        TouchData touch_data = {evt.unique_event_id(), rwhv, /*acked*/ false,
                                /*result*/ ui::ER_UNHANDLED};
        touch_queue_.push_back(touch_data);

        handler->OnTouchEvent(&evt);
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
            base::TimeTicks() + base::Microseconds(ev.timestamp()), ev.flags(),
            mouse.changed_button_flags());
        if (contents->GetAccessibilityMode().has_mode(
                ui::AXMode::kWebContents)) {
          evt.set_flags(evt.flags() | ui::EF_TOUCH_ACCESSIBILITY);
        }
        handler->OnMouseEvent(&evt);
      } else {
        client_->OnError("mouse() not supplied for mouse event");
      }
      break;
    case ui::ET_KEY_PRESSED:
    case ui::ET_KEY_RELEASED:
      if (ev.has_key()) {
        ui::DomKey dom_key =
            ui::KeycodeConverter::KeyStringToDomKey(ev.key().key_string());

        bool send_keypress = false;
        ui::DomCode dom_code = UsLayoutDomKeyToDomCode(dom_key);
        ui::KeyboardCode keyboard_code =
            DomCodeToUsLayoutNonLocatedKeyboardCode(dom_code);

        if (dom_key.IsCharacter())
          send_keypress = true;

        // Required to match desktop.
        if (keyboard_code == ui::VKEY_BACK || (keyboard_code == ui::VKEY_TAB))
          send_keypress = false;

        if (dom_code == ui::DomCode::NONE) {
          if (type != ui::ET_KEY_PRESSED)
            return;
          // Non-US layout keys should only generate a keypress event.
          ui::KeyEvent key_press(type, keyboard_code, dom_code, ev.flags(),
                                 dom_key,
                                 TimeTicksFromTimestamp(ev.timestamp()), true);
          handler->OnKeyEvent(&key_press);
          return;
        }

        // Generate the keydown or keyup event.
        ui::KeyEvent key_event(type, keyboard_code, dom_code, ev.flags(),
                               dom_key, TimeTicksFromTimestamp(ev.timestamp()),
                               false);
        handler->OnKeyEvent(&key_event);

        if (key_event.stopped_propagation())
          return;

        if (send_keypress && type == ui::ET_KEY_PRESSED) {
          // Generate the keypress event.
          ui::KeyEvent key_press(type, keyboard_code, dom_code, ev.flags(),
                                 dom_key,
                                 TimeTicksFromTimestamp(ev.timestamp()), true);
          handler->OnKeyEvent(&key_press);
        }
      } else {
        client_->OnError("key() not supplied for key event");
      }
      break;
    default:
      break;
  }
}

void WebContentController::RegisterRenderWidgetInputObserverFromRenderFrameHost(
    WebContentController* web_content_controller,
    content::RenderFrameHost* render_frame_host) {
  content::RenderWidgetHostView* view = render_frame_host->GetView();
  if (view) {
    web_content_controller->RegisterRenderWidgetInputObserver(
        view->GetRenderWidgetHost());
  }
}

void WebContentController::RegisterRenderWidgetInputObserver(
    content::RenderWidgetHost* render_widget_host) {
  auto insertion = current_render_widget_set_.insert(render_widget_host);
  if (insertion.second) {
    render_widget_host->AddInputEventObserver(this);
  }
}

void WebContentController::UnregisterRenderWidgetInputObserver(
    content::RenderWidgetHost* render_widget_host) {
  current_render_widget_set_.erase(render_widget_host);
  render_widget_host->RemoveInputEventObserver(this);
}

void WebContentController::JavascriptCallback(int64_t id, base::Value result) {
  std::string json;
  base::JSONWriter::Write(result, &json);
  std::unique_ptr<webview::WebviewResponse> response =
      std::make_unique<webview::WebviewResponse>();
  response->set_id(id);
  response->mutable_evaluate_javascript()->set_json(json);

  // Async response may come after Destroy() was called but before the web page
  // closed.
  if (client_)
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

  // Remove disk cache and local storage.
  content::BrowsingDataRemover* remover =
      GetWebContents()->GetBrowserContext()->GetBrowsingDataRemover();
  remover->Remove(base::Time(), base::Time::Max(),
                  content::BrowsingDataRemover::DATA_TYPE_CACHE |
                      content::BrowsingDataRemover::DATA_TYPE_DOM_STORAGE,
                  content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
                      content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB);
}

void WebContentController::HandleClearCookies(int64_t id) {
  std::unique_ptr<webview::WebviewResponse> response =
      std::make_unique<webview::WebviewResponse>();

  content::BrowsingDataRemover* remover =
      GetWebContents()->GetBrowserContext()->GetBrowsingDataRemover();
  remover->Remove(base::Time(), base::Time::Max(),
                  content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                  content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
                      content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB);

  // There appears to be no way of knowing if this actually clears anything.
  response->mutable_clear_cookies()->set_had_cookies(false);
  response->set_id(id);
  client_->EnqueueSend(std::move(response));
}

void WebContentController::HandleGetTitle(int64_t id) {
  std::unique_ptr<webview::WebviewResponse> response =
      std::make_unique<webview::WebviewResponse>();

  response->set_id(id);
  response->mutable_get_title()->set_title(
      base::UTF16ToUTF8(GetWebContents()->GetTitle()));
  client_->EnqueueSend(std::move(response));
}

void WebContentController::HandleResize(const gfx::Size& size) {
  LOG(INFO) << "Sizing web content to " << size.ToString();
  GetWebContents()->GetNativeView()->SetBounds(gfx::Rect(size));
  if (surface_) {
    surface_->SetEmbeddedSurfaceSize(size);
    surface_->Commit();
  }
}

void WebContentController::HandleSetInsets(const gfx::Insets& insets) {
  auto* contents = GetWebContents();
  if (contents && contents->GetTopLevelRenderWidgetHostView())
    contents->GetTopLevelRenderWidgetHostView()->SetInsets(insets);
}

void WebContentController::HandleGetUserAgent(int64_t id) {
  std::unique_ptr<webview::WebviewResponse> response =
      std::make_unique<webview::WebviewResponse>();

  response->set_id(id);
  response->mutable_get_user_agent()->set_user_agent(GetUserAgent());
  client_->EnqueueSend(std::move(response));
}

viz::SurfaceId WebContentController::GetSurfaceId() {
  content::WebContents* web_contents = GetWebContents();
  // Web contents are destroyed before controller for cast apps.
  if (!web_contents)
    return viz::SurfaceId();
  auto* rwhv = web_contents->GetRenderWidgetHostView();
  if (!rwhv)
    return viz::SurfaceId();
  auto frame_sink_id = rwhv->GetRenderWidgetHost()->GetFrameSinkId();
  auto local_surface_id = rwhv->GetNativeView()->GetLocalSurfaceId();
  return viz::SurfaceId(frame_sink_id, local_surface_id);
}

void WebContentController::OnSurfaceDestroying(exo::Surface* surface) {
  DCHECK_EQ(surface, surface_);
  surface->RemoveSurfaceObserver(this);
  surface_ = nullptr;
}

void WebContentController::PrimaryMainFrameWasResized(bool width_changed) {
  // The surface ID may have changed, so trigger a new commit to re-issue the
  // draw quad.
  if (surface_) {
    surface_->Commit();
  }
}

void WebContentController::FrameSizeChanged(
    content::RenderFrameHost* render_frame_host,
    const gfx::Size& frame_size) {
  // The surface ID may have changed, so trigger a new commit to re-issue the
  // draw quad.
  if (surface_) {
    surface_->Commit();
  }
}

void WebContentController::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  current_render_frame_set_.insert(render_frame_host);

  if (!render_frame_host->GetParent())
    RegisterRenderWidgetInputObserver(render_frame_host->GetRenderWidgetHost());

  auto* instance =
      JsClientInstance::Find(render_frame_host->GetProcess()->GetID(),
                             render_frame_host->GetRoutingID());
  // If the instance doesn't exist yet the JsClientInstance observer will see
  // it later on.
  if (instance)
    SendInitialChannelSet(instance);
  content::RenderWidgetHostView* view = render_frame_host->GetView();
  if (view) {
    RegisterRenderWidgetInputObserver(view->GetRenderWidgetHost());
  }
}

void WebContentController::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  current_render_frame_set_.erase(render_frame_host);

  if (!render_frame_host->GetParent()) {
    content::RenderWidgetHost* rwh = render_frame_host->GetRenderWidgetHost();
    UnregisterRenderWidgetInputObserver(rwh);
    content::RenderWidgetHostView* rwhv = render_frame_host->GetView();
    base::EraseIf(touch_queue_,
                  [rwhv](TouchData data) { return data.rwhv == rwhv; });
  }
}

void WebContentController::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  // The surface ID may have changed, so trigger a new commit to re-issue the
  // draw quad.
  if (surface_) {
    surface_->Commit();
  }
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

void WebContentController::AckTouchEvent(content::RenderWidgetHostView* rwhv,
                                         uint32_t unique_event_id,
                                         ui::EventResult result) {
  // GestureRecognizerImpl makes AckTouchEvent private so cast to the interface.
  ui::GestureRecognizer* recognizer = &gesture_recognizer_;
  auto list = recognizer->AckTouchEvent(unique_event_id, result, false,
                                        rwhv->GetNativeView());
  // Forward any resulting gestures.
  ui::EventHandler* handler = rwhv->GetNativeView()->delegate();
  for (auto& e : list) {
    handler->OnGestureEvent(e.get());
  }
}

void WebContentController::OnInputEventAck(
    blink::mojom::InputEventResultSource source,
    blink::mojom::InputEventResultState state,
    const blink::WebInputEvent& e) {
  if (!blink::WebInputEvent::IsTouchEventType(e.GetType()))
    return;
  const uint32_t id =
      static_cast<const blink::WebTouchEvent&>(e).unique_touch_event_id;
  ui::EventResult result =
      state == blink::mojom::InputEventResultState::kConsumed
          ? ui::ER_HANDLED
          : ui::ER_UNHANDLED;
  const auto it = find_if(touch_queue_.begin(), touch_queue_.end(),
                          [id](TouchData data) { return data.id == id; });
  if (it == touch_queue_.end()) {
    content::WebContents* contents = GetWebContents();
    content::RenderWidgetHostView* rwhv = contents->GetRenderWidgetHostView();
    AckTouchEvent(rwhv, id, result);
  } else {
    // Record the ack.
    it->acked = true;
    it->result = result;
    // Handle any available acks.
    while (!touch_queue_.empty() && touch_queue_.front().acked) {
      TouchData data = touch_queue_.front();
      touch_queue_.pop_front();
      AckTouchEvent(data.rwhv, data.id, data.result);
    }
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
  // Calls may come after Destroy() was called but before the web page closed.
  if (!js_channels_)
    return;

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
