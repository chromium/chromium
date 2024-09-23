// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plugins/renderer/webview_plugin.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "gin/converter.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/page/browsing_context_group_info.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom.h"
#include "third_party/blink/public/mojom/page/prerender_page_param.mojom.h"
#include "third_party/blink/public/mojom/partitioned_popins/partitioned_popin_params.mojom.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/web_policy_container.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_navigation_control.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/public/web/web_view.h"

using blink::DragOperationsMask;
using blink::WebDragData;
using blink::WebFrameWidget;
using blink::WebLocalFrame;
using blink::WebMouseEvent;
using blink::WebPlugin;
using blink::WebPluginContainer;
using blink::WebString;
using blink::WebURLError;
using blink::WebURLResponse;
using blink::WebVector;
using blink::WebView;
using blink::web_pref::WebPreferences;

WebViewPlugin::WebViewPlugin(WebView* web_view,
                             WebViewPlugin::Delegate* delegate,
                             const WebPreferences& preferences)
    : blink::WebViewObserver(web_view),
      delegate_(delegate),
      container_(nullptr),
      finished_loading_(false),
      focused_(false),
      is_painting_(false),
      is_resizing_(false),
      web_view_helper_(this, preferences, web_view->GetRendererPreferences()) {}

// static
WebViewPlugin* WebViewPlugin::Create(WebView* web_view,
                                     WebViewPlugin::Delegate* delegate,
                                     const WebPreferences& preferences,
                                     const std::string& html_data,
                                     const GURL& url) {
  DCHECK(url.is_valid()) << "Blink requires the WebView to have a valid URL.";
  WebViewPlugin* plugin = new WebViewPlugin(web_view, delegate, preferences);
  // Loading may synchronously access |delegate| which could be
  // uninitialized just yet, so load in another task.
  plugin->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&WebViewPlugin::LoadHTML,
                     plugin->weak_factory_.GetWeakPtr(), html_data, url));
  return plugin;
}

WebViewPlugin::~WebViewPlugin() {
  DCHECK(!weak_factory_.HasWeakPtrs());
}

void WebViewPlugin::ReplayReceivedData(WebPlugin* plugin) {
  if (!response_.IsNull()) {
    plugin->DidReceiveResponse(response_);
    for (auto it = data_.begin(); it != data_.end(); ++it) {
      plugin->DidReceiveData(*it);
    }
  }
  // We need to transfer the |focused_| to new plugin after it loaded.
  if (focused_)
    plugin->UpdateFocus(true, blink::mojom::FocusType::kNone);
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

  web_view()->MainFrameWidget()->SetZoomLevel(
      blink::ZoomFactorToZoomLevel(container_->LayoutZoomFactor()));

  return true;
}

void WebViewPlugin::Destroy() {
  weak_factory_.InvalidateWeakPtrs();

  if (delegate_) {
    delegate_->PluginDestroyed();
    delegate_ = nullptr;
  }
  container_ = nullptr;
  blink::WebViewObserver::Observe(nullptr);
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                this);
}

v8::Local<v8::Object> WebViewPlugin::V8ScriptableObject(v8::Isolate* isolate) {
  if (!delegate_)
    return v8::Local<v8::Object>();

  return delegate_->GetV8ScriptableObject(isolate);
}

void WebViewPlugin::UpdateAllLifecyclePhases(
    blink::DocumentUpdateReason reason) {
  DCHECK(web_view()->MainFrameWidget());
  web_view()->MainFrameWidget()->UpdateAllLifecyclePhases(reason);
}

bool WebViewPlugin::IsErrorPlaceholder() {
  if (!delegate_)
    return false;
  return delegate_->IsErrorPlaceholder();
}

void WebViewPlugin::Paint(cc::PaintCanvas* canvas, const gfx::Rect& rect) {
  gfx::Rect paint_rect = gfx::IntersectRects(rect_, rect);
  if (paint_rect.IsEmpty())
    return;

  base::AutoReset<bool> is_painting(
        &is_painting_, true);

  paint_rect.Offset(-rect_.x(), -rect_.y());

  canvas->save();
  canvas->translate(SkIntToScalar(rect_.x()), SkIntToScalar(rect_.y()));
  web_view()->MainFrameWidget()->UpdateLifecycle(
      blink::WebLifecycleUpdate::kAll,
      blink::DocumentUpdateReason::kBeginMainFrame);
  web_view()->PaintContent(canvas, paint_rect);
  canvas->restore();
}

// Coordinates are relative to the containing window.
void WebViewPlugin::UpdateGeometry(const gfx::Rect& window_rect,
                                   const gfx::Rect& clip_rect,
                                   const gfx::Rect& unobscured_rect,
                                   bool is_visible) {
  DCHECK(container_);

  base::AutoReset<bool> is_resizing(&is_resizing_, true);

  if (window_rect != rect_) {
    rect_ = window_rect;
    DCHECK(web_view()->MainFrameWidget());
    web_view()->MainFrameWidget()->Resize(rect_.size());
  }

  // Plugin updates are forbidden during Blink layout. Therefore,
  // UpdatePluginForNewGeometry must be posted to a task to run asynchronously.
  web_view_helper_.main_frame()
      ->GetAgentGroupScheduler()
      ->CompositorTaskRunner()
      ->PostTask(FROM_HERE,
                 base::BindOnce(&WebViewPlugin::UpdatePluginForNewGeometry,
                                weak_factory_.GetWeakPtr(), window_rect,
                                unobscured_rect));
}

void WebViewPlugin::UpdateFocus(bool focused,
                                blink::mojom::FocusType focus_type) {
  focused_ = focused;
}

blink::WebInputEventResult WebViewPlugin::HandleInputEvent(
    const blink::WebCoalescedInputEvent& coalesced_event,
    ui::Cursor* cursor) {
  const blink::WebInputEvent& event = coalesced_event.Event();
  // For tap events, don't handle them. They will be converted to
  // mouse events later and passed to here.
  if (event.GetType() == blink::WebInputEvent::Type::kGestureTap)
    return blink::WebInputEventResult::kNotHandled;

  // For LongPress events we return false, since otherwise the context menu will
  // be suppressed. https://crbug.com/482842
  if (event.GetType() == blink::WebInputEvent::Type::kGestureLongPress)
    return blink::WebInputEventResult::kNotHandled;

  if (event.GetType() == blink::WebInputEvent::Type::kContextMenu) {
    if (delegate_) {
      const WebMouseEvent& mouse_event =
          static_cast<const WebMouseEvent&>(event);
      delegate_->ShowContextMenu(mouse_event);
    }
    return blink::WebInputEventResult::kHandledSuppressed;
  }
  current_cursor_ = *cursor;
  DCHECK(web_view()->MainFrameWidget());
  blink::WebInputEventResult handled =
      web_view()->MainFrameWidget()->HandleInputEvent(coalesced_event);
  *cursor = current_cursor_;

  return handled;
}

void WebViewPlugin::DidReceiveResponse(const WebURLResponse& response) {
  DCHECK(response_.IsNull());
  response_ = response;
}

void WebViewPlugin::DidReceiveData(base::span<const char> data) {
  data_.push_back(std::string(data.data(), data.size()));
}

void WebViewPlugin::DidFinishLoading() {
  DCHECK(!finished_loading_);
  finished_loading_ = true;
}

void WebViewPlugin::DidFailLoading(const WebURLError& error) {
  DCHECK(!error_);
  error_ = std::make_unique<WebURLError>(error);
}

WebViewPlugin::WebViewHelper::WebViewHelper(
    WebViewPlugin* plugin,
    const WebPreferences& parent_web_preferences,
    const blink::RendererPreferences& parent_renderer_preferences)
    : plugin_(plugin),
      agent_group_scheduler_(
          blink::scheduler::WebThreadScheduler::MainThreadScheduler()
              .CreateWebAgentGroupScheduler()) {
  web_view_ = WebView::Create(
      /*client=*/this,
      /*is_hidden=*/false,
      /*prerender_param=*/nullptr,
      /*fenced_frame_mode=*/std::nullopt,
      /*compositing_enabled=*/false,
      /*widgets_never_composited=*/false,
      /*opener=*/nullptr, mojo::NullAssociatedReceiver(),
      *agent_group_scheduler_,
      /*session_storage_namespace_id=*/std::string(),
      /*page_base_background_color=*/std::nullopt,
      blink::BrowsingContextGroupInfo::CreateUnique(),
      /*color_provider_colors=*/nullptr,
      /*partitioned_popin_params=*/nullptr);
  // ApplyWebPreferences before making a WebLocalFrame so that the frame sees a
  // consistent view of our preferences.
  blink::WebView::ApplyWebPreferences(parent_web_preferences, web_view_);

  // Turn off AcceptLoadDrops for this plugin webview.
  blink::RendererPreferences renderer_preferences = parent_renderer_preferences;
  renderer_preferences.can_accept_load_drops = false;
  web_view_->SetRendererPreferences(renderer_preferences);

  WebLocalFrame* web_frame = WebLocalFrame::CreateMainFrame(
      web_view_, this, nullptr, mojo::NullRemote(), blink::LocalFrameToken(),
      blink::DocumentToken(), nullptr);
  blink::WebFrameWidget* frame_widget = web_frame->InitializeFrameWidget(
      blink::CrossVariantMojoAssociatedRemote<
          blink::mojom::FrameWidgetHostInterfaceBase>(),
      blink::CrossVariantMojoAssociatedReceiver<
          blink::mojom::FrameWidgetInterfaceBase>(),
      blink_widget_host_receiver_.BindNewEndpointAndPassDedicatedRemote(),
      blink_widget_.BindNewEndpointAndPassDedicatedReceiver(),
      viz::FrameSinkId());
  frame_widget->InitializeNonCompositing(this);
  frame_widget->DisableDragAndDrop();

  // The WebFrame created here was already attached to the Page as its main
  // frame, and the WebFrameWidget has been initialized, so we can call
  // WebView's DidAttachLocalMainFrame().
  web_view_->DidAttachLocalMainFrame();
}

WebViewPlugin::WebViewHelper::~WebViewHelper() {
  web_view_->Close();
}

void WebViewPlugin::WebViewHelper::UpdateTooltipUnderCursor(
    const std::u16string& tooltip_text,
    base::i18n::TextDirection hint) {
  UpdateTooltip(tooltip_text);
}

void WebViewPlugin::WebViewHelper::UpdateTooltipFromKeyboard(
    const std::u16string& tooltip_text,
    base::i18n::TextDirection hint,
    const gfx::Rect& bounds) {
  UpdateTooltip(tooltip_text);
}

void WebViewPlugin::WebViewHelper::ClearKeyboardTriggeredTooltip() {
  // This is an exception to the "only clear it if its set from keyboard" since
  // there are no way of knowing whether the tooltips were set from keyboard or
  // cursor in this class. In any case, this will clear the tooltip.
  UpdateTooltip(std::u16string());
}

void WebViewPlugin::WebViewHelper::UpdateTooltip(
    const std::u16string& tooltip_text) {
  if (plugin_->container_) {
    plugin_->container_->GetElement().SetAttribute(
        "title", WebString::FromUTF16(tooltip_text));
  }
}

void WebViewPlugin::WebViewHelper::InvalidateContainer() {
  if (plugin_->container_)
    plugin_->container_->Invalidate();
}

void WebViewPlugin::WebViewHelper::SetCursor(const ui::Cursor& cursor) {
  plugin_->current_cursor_ = cursor;
}

void WebViewPlugin::WebViewHelper::ScheduleNonCompositedAnimation() {
  // Resizes must be self-contained: any lifecycle updating must
  // be triggerd from within the WebView or this WebViewPlugin.
  // This is because this WebViewPlugin is contained in another
  // WebView which may be in the middle of updating its lifecycle,
  // but after layout is done, and it is illegal to dirty earlier
  // lifecycle stages during later ones.
  if (plugin_->is_resizing_)
    return;
  if (plugin_->container_) {
    // This should never happen; see also crbug.com/545039 for context.
    DCHECK(!plugin_->is_painting_);
    // This goes to compositor of the containing frame.
    plugin_->container_->ScheduleAnimation();
  }
}

scoped_refptr<network::SharedURLLoaderFactory>
WebViewPlugin::WebViewHelper::GetURLLoaderFactory() {
  return plugin_->Container()
      ->GetDocument()
      .GetFrame()
      ->Client()
      ->GetURLLoaderFactory();
}

void WebViewPlugin::WebViewHelper::BindToFrame(
    blink::WebNavigationControl* frame) {
  frame_ = frame;
}

void WebViewPlugin::WebViewHelper::DidClearWindowObject() {
  if (!plugin_->delegate_)
    return;

  v8::Isolate* isolate = frame_->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame_->MainWorldScriptContext();
  DCHECK(!context.IsEmpty());
  v8::MicrotasksScope microtasks_scope(
      isolate, context->GetMicrotaskQueue(),
      v8::MicrotasksScope::kDoNotRunMicrotasks);

  v8::Context::Scope context_scope(context);
  v8::Local<v8::Object> global = context->Global();

  global
      ->Set(context, gin::StringToV8(isolate, "plugin"),
            plugin_->delegate_->GetV8Handle(isolate))
      .Check();
}

void WebViewPlugin::WebViewHelper::FrameDetached() {
  frame_->Close();
  frame_ = nullptr;
}

void WebViewPlugin::OnZoomLevelChanged() {
  if (container_) {
    web_view()->MainFrameWidget()->SetZoomLevel(
        blink::ZoomFactorToZoomLevel(container_->LayoutZoomFactor()));
  }
}

void WebViewPlugin::LoadHTML(const std::string& html_data, const GURL& url) {
  auto params = std::make_unique<blink::WebNavigationParams>();
  params->url = url;
  params->policy_container = std::make_unique<blink::WebPolicyContainer>();

  // The |html_data| comes from files in: chrome/renderer/resources/plugins/
  // Executing scripts is the only capability required.
  //
  // WebSandboxFlags is a bit field. This removes all the capabilities, except
  // script execution.
  using network::mojom::WebSandboxFlags;
  params->policy_container->policies.sandbox_flags =
      static_cast<WebSandboxFlags>(
          ~static_cast<int>(WebSandboxFlags::kScripts));
  blink::WebNavigationParams::FillStaticResponse(params.get(), "text/html",
                                                 "UTF-8", html_data);
  web_view_helper_.main_frame()->CommitNavigation(std::move(params),
                                                  /*extra_data=*/nullptr);
}

void WebViewPlugin::UpdatePluginForNewGeometry(
    const gfx::Rect& window_rect,
    const gfx::Rect& unobscured_rect) {
  DCHECK(container_);
  if (!delegate_)
    return;

  // The delegate may instantiate a new plugin.
  delegate_->OnUnobscuredRectUpdate(gfx::Rect(unobscured_rect));
  // The delegate may have dirtied style and layout of the WebView.
  // Run the lifecycle now so that it is clean.
  DCHECK(web_view()->MainFrameWidget());
  web_view()->MainFrameWidget()->UpdateAllLifecyclePhases(
      blink::DocumentUpdateReason::kPlugin);
}

scoped_refptr<base::SingleThreadTaskRunner> WebViewPlugin::GetTaskRunner() {
  return web_view_helper_.main_frame()->GetTaskRunner(
      blink::TaskType::kInternalDefault);
}
