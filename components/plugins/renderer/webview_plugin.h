// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUGINS_RENDERER_WEBVIEW_PLUGIN_H_
#define COMPONENTS_PLUGINS_RENDERER_WEBVIEW_PLUGIN_H_

#include <list>
#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/blink/public/platform/web_cursor_info.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_navigation_control.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/public/web/web_widget_client.h"

namespace blink {
class WebLocalFrame;
class WebMouseEvent;
}

namespace content {
class RenderView;
struct WebPreferences;
}

// This class implements the WebPlugin interface by forwarding drawing and
// handling input events to a WebView.
// It can be used as a placeholder for an actual plugin, using HTML for the UI.
// To show HTML data inside the WebViewPlugin,
// call web_view->mainFrame()->loadHTMLString() with the HTML data and a fake
// chrome:// URL as origin.

class WebViewPlugin : public blink::WebPlugin,
                      public content::RenderViewObserver {
 public:
  class Delegate {
   public:
    // Called to get the V8 handle used to bind the lifetime to the frame.
    virtual v8::Local<v8::Value> GetV8Handle(v8::Isolate*) = 0;

    // Called upon a context menu event.
    virtual void ShowContextMenu(const blink::WebMouseEvent&) = 0;

    // Called when the WebViewPlugin is destroyed.
    virtual void PluginDestroyed() = 0;

    // Called to enable JavaScript pass-through to a throttled plugin, which is
    // loaded but idle. Doesn't work for blocked plugins, which is not loaded.
    virtual v8::Local<v8::Object> GetV8ScriptableObject(v8::Isolate*) const = 0;

    // Called when the unobscured rect of the plugin is updated.
    virtual void OnUnobscuredRectUpdate(const gfx::Rect& unobscured_rect) {}

    virtual bool IsErrorPlaceholder() = 0;
  };

  // Convenience method to set up a new WebViewPlugin using |preferences|
  // and displaying |html_data|. |url| should be a (fake) data:text/html URL;
  // it is only used for navigation and never actually resolved.
  static WebViewPlugin* Create(content::RenderView* render_view,
                               Delegate* delegate,
                               const content::WebPreferences& preferences,
                               const std::string& html_data,
                               const GURL& url);

  blink::WebLocalFrame* main_frame() { return web_view_helper_.main_frame(); }

  const blink::WebString& old_title() const { return old_title_; }

  // When loading a plugin document (i.e. a full page plugin not embedded in
  // another page), we save all data that has been received, and replay it with
  // this method on the actual plugin.
  void ReplayReceivedData(blink::WebPlugin* plugin);

  // WebPlugin methods:
  blink::WebPluginContainer* Container() const override;
  // The WebViewPlugin, by design, never fails to initialize. It's used to
  // display placeholders and error messages, so it must never fail.
  bool Initialize(blink::WebPluginContainer*) override;
  void Destroy() override;

  v8::Local<v8::Object> V8ScriptableObject(v8::Isolate* isolate) override;

  bool IsErrorPlaceholder() override;

  void UpdateAllLifecyclePhases(
      blink::WebWidget::LifecycleUpdateReason reason) override;
  void Paint(cc::PaintCanvas* canvas, const blink::WebRect& rect) override;

  // Coordinates are relative to the containing window.
  void UpdateGeometry(const blink::WebRect& window_rect,
                      const blink::WebRect& clip_rect,
                      const blink::WebRect& unobscured_rect,
                      bool is_visible) override;

  void UpdateFocus(bool foucsed, blink::WebFocusType focus_type) override;
  void UpdateVisibility(bool) override {}

  blink::WebInputEventResult HandleInputEvent(
      const blink::WebCoalescedInputEvent& event,
      blink::WebCursorInfo& cursor_info) override;

  void DidReceiveResponse(const blink::WebURLResponse& response) override;
  void DidReceiveData(const char* data, size_t data_length) override;
  void DidFinishLoading() override;
  void DidFailLoading(const blink::WebURLError& error) override;

 private:
  friend class base::DeleteHelper<WebViewPlugin>;
  WebViewPlugin(content::RenderView* render_view,
                Delegate* delegate,
                const content::WebPreferences& preferences);
  ~WebViewPlugin() override;

  blink::WebView* web_view() { return web_view_helper_.web_view(); }

  // content::RenderViewObserver methods:
  void OnDestruct() override {}
  void OnZoomLevelChanged() override;

  void LoadHTML(const std::string& html_data, const GURL& url);
  void UpdatePluginForNewGeometry(const blink::WebRect& window_rect,
                                  const blink::WebRect& unobscured_rect);

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner();

  // Manages its own lifetime.
  Delegate* delegate_;

  blink::WebCursorInfo current_cursor_;

  // Owns us.
  blink::WebPluginContainer* container_;

  gfx::Rect rect_;

  blink::WebURLResponse response_;
  std::list<std::string> data_;
  std::unique_ptr<blink::WebURLError> error_;
  blink::WebString old_title_;
  bool finished_loading_;
  bool focused_;
  bool is_painting_;
  bool is_resizing_;

  // A helper that handles interaction from WebViewPlugin's internal WebView.
  class WebViewHelper : public blink::WebViewClient,
                        public blink::WebWidgetClient,
                        public blink::WebLocalFrameClient {
   public:
    WebViewHelper(WebViewPlugin* plugin,
                  const content::WebPreferences& preferences);
    ~WebViewHelper() override;

    blink::WebView* web_view() { return web_view_; }
    blink::WebNavigationControl* main_frame() { return frame_; }

    // WebViewClient methods:
    bool AcceptsLoadDrops() override;
    bool CanHandleGestureEvent() override;
    bool CanUpdateLayout() override;
    blink::WebScreenInfo GetScreenInfo() override;
    void DidInvalidateRect(const blink::WebRect&) override;

    // WebWidgetClient methods:
    void SetToolTipText(const blink::WebString&,
                        blink::WebTextDirection) override;
    void StartDragging(network::mojom::ReferrerPolicy,
                       const blink::WebDragData&,
                       blink::WebDragOperationsMask,
                       const SkBitmap&,
                       const gfx::Point&) override;
    void DidChangeCursor(const blink::WebCursorInfo& cursor) override;
    void ScheduleAnimation() override;

    // WebLocalFrameClient methods:
    void BindToFrame(blink::WebNavigationControl* frame) override;
    void DidClearWindowObject() override;
    void FrameDetached(DetachType) override;
    std::unique_ptr<blink::WebURLLoaderFactory> CreateURLLoaderFactory()
        override;

   private:
    WebViewPlugin* plugin_;
    blink::WebNavigationControl* frame_ = nullptr;

    // Owned by us, deleted via |close()|.
    blink::WebView* web_view_;
  };
  WebViewHelper web_view_helper_;

  // Should be invalidated when destroy() is called.
  base::WeakPtrFactory<WebViewPlugin> weak_factory_{this};
};

#endif  // COMPONENTS_PLUGINS_RENDERER_WEBVIEW_PLUGIN_H_
