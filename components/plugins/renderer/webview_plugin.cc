// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plugins/renderer/webview_plugin.h"

#include <stddef.h>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/common/web_preferences.h"
#include "content/public/renderer/render_view.h"
#include "gin/converter.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom.h"
#include "third_party/blink/public/platform/web_coalesced_input_event.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/public/web/web_view.h"

using blink::WebCursorInfo;
using blink::WebDragData;
using blink::WebDragOperationsMask;
using blink::WebFrameWidget;
using blink::WebLocalFrame;
using blink::WebMouseEvent;
using blink::WebPlugin;
using blink::WebPluginContainer;
using blink::WebPoint;
using blink::WebRect;
using blink::WebString;
using blink::WebURLError;
using blink::WebURLResponse;
using blink::WebVector;
using blink::WebView;
using content::WebPreferences;

WebViewPlugin::WebViewPlugin(content::RenderView* render_view,
                             WebViewPlugin::Delegate* delegate,
                             const WebPreferences& preferences)
    : content::RenderViewObserver(render_view),
      delegate_(delegate),
      container_(nullptr),
      finished_loading_(false),
      focused_(false),
      is_painting_(false),
      is_resizing_(false),
      web_view_helper_(this, preferences),
      weak_factory_(this) {
}

// static
WebViewPlugin* WebViewPlugin::Create(content::RenderView* render_view,
                                     WebViewPlugin::Delegate* delegate,
                                     const WebPreferences& preferences,
                                     const std::string& html_data,
                                     const GURL& url) {
  DCHECK(url.is_valid()) << "Blink requires the WebView to have a valid URL.";
  WebViewPlugin* plugin = new WebViewPlugin(render_view, delegate, preferences);
  plugin->main_frame()->LoadHTMLString(html_data, url);
  return plugin;
}

WebViewPlugin::~WebViewPlugin() {
  DCHECK(!weak_factory_.HasWeakPtrs());
}

void WebViewPlugin::ReplayReceivedData(WebPlugin* plugin) {
  if (!response_.IsNull()) {
    plugin->DidReceiveResponse(response_);
    size_t total_bytes = 0;
    for (auto it = data_.begin(); it != data_.end(); ++it) {
      plugin->DidReceiveData(it->c_str(), it->length());
      total_bytes += it->length();
    }
  }
  // We need to transfer the |focused_| to new plugin after it loaded.
  if (focused_)
    plugin->UpdateFocus(true, blink::kWebFocusTypeNone);
  if (finished_loading_)
    plugin->DidFinishLoading();
  if (error_)
    plugin->DidFailLoading(*error_);
}

WebPluginContainer* WebViewPlugin::Container() const {
  return container_;
}

bool WebViewPlugin::Initialize(WebPluginContainer* container) {
  DCHECK(container);
  DCHECK_EQ(this, container->Plugin());
  container_ = container;

  // We must call layout again here to ensure that the container is laid
  // out before we next try to paint it, which is a requirement of the
  // document life cycle in Blink. In most cases, needsLayout is set by
  // scheduleAnimation, but due to timers controlling widget update,
  // scheduleAnimation may be invoked before this initialize call (which
  // comes through the widget update process). It doesn't hurt to mark
  // for animation again, and it does help us in the race-condition situation.
  container_->ScheduleAnimation();

  old_title_ = container_->GetElement().GetAttribute("title");

  // Propagate device scale and zoom level to inner webview.
  web_view()->SetDeviceScaleFactor(container_->DeviceScaleFactor());
  web_view()->SetZoomLevel(
      blink::WebView::ZoomFactorToZoomLevel(container_->PageZoomFactor()));

  return true;
}

void WebViewPlugin::Destroy() {
  weak_factory_.InvalidateWeakPtrs();

  if (delegate_) {
    delegate_->PluginDestroyed();
    delegate_ = nullptr;
  }
  container_ = nullptr;
  content::RenderViewObserver::Observe(nullptr);
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

v8::Local<v8::Object> WebViewPlugin::V8ScriptableObject(v8::Isolate* isolate) {
  if (!delegate_)
    return v8::Local<v8::Object>();

  return delegate_->GetV8ScriptableObject(isolate);
}

void WebViewPlugin::UpdateAllLifecyclePhases() {
  web_view()->UpdateAllLifecyclePhases();
}

bool WebViewPlugin::IsErrorPlaceholder() {
  if (!delegate_)
    return false;
  return delegate_->IsErrorPlaceholder();
}

void WebViewPlugin::Paint(cc::PaintCanvas* canvas, const WebRect& rect) {
  gfx::Rect paint_rect = gfx::IntersectRects(rect_, rect);
  if (paint_rect.IsEmpty())
    return;

  base::AutoReset<bool> is_painting(
        &is_painting_, true);

  paint_rect.Offset(-rect_.x(), -rect_.y());

  canvas->save();
  canvas->translate(SkIntToScalar(rect_.x()), SkIntToScalar(rect_.y()));

  // Apply inverse device scale factor, as the outer webview has already
  // applied it, and the inner webview will apply it again.
  SkScalar inverse_scale =
      SkFloatToScalar(1.0 / container_->DeviceScaleFactor());
  canvas->scale(inverse_scale, inverse_scale);

  web_view()->PaintContent(canvas, paint_rect);

  canvas->restore();
}

// Coordinates are relative to the containing window.
void WebViewPlugin::UpdateGeometry(const WebRect& window_rect,
                                   const WebRect& clip_rect,
                                   const WebRect& unobscured_rect,
                                   bool is_visible) {
  DCHECK(container_);

  base::AutoReset<bool> is_resizing(&is_resizing_, true);

  if (static_cast<gfx::Rect>(window_rect) != rect_) {
    rect_ = window_rect;
    web_view()->Resize(rect_.size());
  }

  // Plugin updates are forbidden during Blink layout. Therefore,
  // UpdatePluginForNewGeometry must be posted to a task to run asynchronously.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&WebViewPlugin::UpdatePluginForNewGeometry,
                     weak_factory_.GetWeakPtr(), window_rect, unobscured_rect));
}

void WebViewPlugin::UpdateFocus(bool focused, blink::WebFocusType focus_type) {
  focused_ = focused;
}

blink::WebInputEventResult WebViewPlugin::HandleInputEvent(
    const blink::WebCoalescedInputEvent& coalesced_event,
    WebCursorInfo& cursor) {
  const blink::WebInputEvent& event = coalesced_event.Event();
  // For tap events, don't handle them. They will be converted to
  // mouse events later and passed to here.
  if (event.GetType() == blink::WebInputEvent::kGestureTap)
    return blink::WebInputEventResult::kNotHandled;

  // For LongPress events we return false, since otherwise the context menu will
  // be suppressed. https://crbug.com/482842
  if (event.GetType() == blink::WebInputEvent::kGestureLongPress)
    return blink::WebInputEventResult::kNotHandled;

  if (event.GetType() == blink::WebInputEvent::kContextMenu) {
    if (delegate_) {
      const WebMouseEvent& mouse_event =
          reinterpret_cast<const WebMouseEvent&>(event);
      delegate_->ShowContextMenu(mouse_event);
    }
    return blink::WebInputEventResult::kHandledSuppressed;
  }
  current_cursor_ = cursor;
  blink::WebInputEventResult handled =
      web_view()->HandleInputEvent(blink::WebCoalescedInputEvent(event));
  cursor = current_cursor_;

  return handled;
}

void WebViewPlugin::DidReceiveResponse(const WebURLResponse& response) {
  DCHECK(response_.IsNull());
  response_ = response;
}

void WebViewPlugin::DidReceiveData(const char* data, size_t data_length) {
  data_.push_back(std::string(data, data_length));
}

void WebViewPlugin::DidFinishLoading() {
  DCHECK(!finished_loading_);
  finished_loading_ = true;
}

void WebViewPlugin::DidFailLoading(const WebURLError& error) {
  DCHECK(!error_);
  error_.reset(new WebURLError(error));
}

WebViewPlugin::WebViewHelper::WebViewHelper(WebViewPlugin* plugin,
                                            const WebPreferences& preferences)
    : plugin_(plugin) {
  web_view_ = WebView::Create(/*client=*/this,
                              /*widget_client=*/this,
                              blink::mojom::PageVisibilityState::kVisible,
                              /*opener=*/nullptr);
  // ApplyWebPreferences before making a WebLocalFrame so that the frame sees a
  // consistent view of our preferences.
  content::RenderView::ApplyWebPreferences(preferences, web_view_);
  WebLocalFrame* web_frame =
      WebLocalFrame::CreateMainFrame(web_view_, this, nullptr, nullptr);
  WebFrameWidget::CreateForMainFrame(this, web_frame);
}

WebViewPlugin::WebViewHelper::~WebViewHelper() {
  web_view_->Close();
}

blink::WebLocalFrame* WebViewPlugin::WebViewHelper::main_frame() {
  // WebViewHelper doesn't support OOPIFs so the main frame will
  // always be local.
  DCHECK(web_view_->MainFrame()->IsWebLocalFrame());
  return static_cast<WebLocalFrame*>(web_view_->MainFrame());
}

bool WebViewPlugin::WebViewHelper::AcceptsLoadDrops() {
  return false;
}

bool WebViewPlugin::WebViewHelper::CanHandleGestureEvent() {
  return true;
}

bool WebViewPlugin::WebViewHelper::CanUpdateLayout() {
  return true;
}

blink::WebScreenInfo WebViewPlugin::WebViewHelper::GetScreenInfo() {
  // TODO(danakj): This should probably return the screen info for the
  // RenderView.
  return blink::WebScreenInfo();
}

blink::WebWidgetClient* WebViewPlugin::WebViewHelper::WidgetClient() {
  return this;
}

void WebViewPlugin::WebViewHelper::SetToolTipText(
    const WebString& text,
    blink::WebTextDirection hint) {
  if (plugin_->container_)
    plugin_->container_->GetElement().SetAttribute("title", text);
}

void WebViewPlugin::WebViewHelper::StartDragging(network::mojom::ReferrerPolicy,
                                                 const WebDragData&,
                                                 WebDragOperationsMask,
                                                 const SkBitmap&,
                                                 const WebPoint&) {
  // Immediately stop dragging.
  main_frame()->FrameWidget()->DragSourceSystemDragEnded();
}

bool WebViewPlugin::WebViewHelper::AllowsBrokenNullLayerTreeView() const {
  return true;
}

void WebViewPlugin::WebViewHelper::DidInvalidateRect(const WebRect& rect) {
  if (plugin_->container_)
    plugin_->container_->InvalidateRect(rect);
}

void WebViewPlugin::WebViewHelper::DidChangeCursor(
    const WebCursorInfo& cursor) {
  plugin_->current_cursor_ = cursor;
}

void WebViewPlugin::WebViewHelper::ScheduleAnimation() {
  // Resizes must be self-contained: any lifecycle updating must
  // be triggerd from within the WebView or this WebViewPlugin.
  // This is because this WebViewPlugin is contained in another
  // Web View which may be in the middle of updating its lifecycle,
  // but after layout is done, and it is illegal to dirty earlier
  // lifecycle stages during later ones.
  if (plugin_->is_resizing_)
    return;
  if (plugin_->container_) {
    // This should never happen; see also crbug.com/545039 for context.
    DCHECK(!plugin_->is_painting_);
    plugin_->container_->ScheduleAnimation();
  }
}

std::unique_ptr<blink::WebURLLoaderFactory>
WebViewPlugin::WebViewHelper::CreateURLLoaderFactory() {
  return blink::Platform::Current()->CreateDefaultURLLoaderFactory();
}

void WebViewPlugin::WebViewHelper::DidClearWindowObject() {
  if (!plugin_->delegate_)
    return;

  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = main_frame()->MainWorldScriptContext();
  DCHECK(!context.IsEmpty());

  v8::Context::Scope context_scope(context);
  v8::Local<v8::Object> global = context->Global();

  global->Set(gin::StringToV8(isolate, "plugin"),
              plugin_->delegate_->GetV8Handle(isolate));
}

void WebViewPlugin::WebViewHelper::FrameDetached(DetachType type) {
  main_frame()->FrameWidget()->Close();
  main_frame()->Close();
}

void WebViewPlugin::OnZoomLevelChanged() {
  if (container_) {
    web_view()->SetZoomLevel(
        blink::WebView::ZoomFactorToZoomLevel(container_->PageZoomFactor()));
  }
}

void WebViewPlugin::UpdatePluginForNewGeometry(
    const blink::WebRect& window_rect,
    const blink::WebRect& unobscured_rect) {
  DCHECK(container_);
  if (!delegate_)
    return;

  // The delegate may instantiate a new plugin.
  delegate_->OnUnobscuredRectUpdate(gfx::Rect(unobscured_rect));
  // The delegate may have dirtied style and layout of the WebView.
  // See for example the resizePoster function in plugin_poster.html.
  // Run the lifecycle now so that it is clean.
  web_view()->UpdateAllLifecyclePhases();
}
