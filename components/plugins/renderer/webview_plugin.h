// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUGINS_RENDERER_WEBVIEW_PLUGIN_H_
#define COMPONENTS_PLUGINS_RENDERER_WEBVIEW_PLUGIN_H_

#include <list>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-forward.h"
#include "third_party/blink/public/mojom/widget/platform_widget.mojom.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_navigation_control.h"
#include "third_party/blink/public/web/web_non_composited_widget_client.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/public/web/web_view_observer.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/ime/mojom/text_input_state.mojom.h"

namespace blink {
namespace web_pref {
struct WebPreferences;
}  // namespace web_pref
struct RendererPreferences;
class WebLocalFrame;
class WebMouseEvent;
}

// This class implements the WebPlugin interface by forwarding drawing and
// handling input events to a WebView.
// It can be used as a placeholder for an actual plugin, using HTML for the UI.
// To show HTML data inside the WebViewPlugin,
// call web_view->mainFrame()->loadHTMLString() with the HTML data and a fake
// chrome:// URL as origin.

class WebViewPlugin : public blink::WebPlugin, public blink::WebViewObserver {
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
  static WebViewPlugin* Create(
      blink::WebView* web_view,
      Delegate* delegate,
      const blink::web_pref::WebPreferences& preferences,
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

  void UpdateAllLifecyclePhases(blink::DocumentUpdateReason reason) override;
  void Paint(cc::PaintCanvas* canvas, const gfx::Rect& rect) override;

  // Coordinates are relative to the containing window.
  void UpdateGeometry(const gfx::Rect& window_rect,
                      const gfx::Rect& clip_rect,
                      const gfx::Rect& unobscured_rect,
                      bool is_visible) override;

  void UpdateFocus(bool foucsed, blink::mojom::FocusType focus_type) override;
  void UpdateVisibility(bool) override {}

  blink::WebInputEventResult HandleInputEvent(
      const blink::WebCoalescedInputEvent& event,
      ui::Cursor* cursor) override;

  void DidReceiveResponse(const blink::WebURLResponse& response) override;
  void DidReceiveData(base::span<const char> data) override;
  void DidFinishLoading() override;
  void DidFailLoading(const blink::WebURLError& error) override;

 private:
  friend class base::DeleteHelper<WebViewPlugin>;
  WebViewPlugin(blink::WebView* web_view,
                Delegate* delegate,
                const blink::web_pref::WebPreferences& preferences);
  ~WebViewPlugin() override;

  blink::WebView* web_view() { return web_view_helper_.web_view(); }

  // blink::WebViewObserver methods:
  void OnDestruct() override {}
  void OnZoomLevelChanged() override;

  void LoadHTML(const std::string& html_data, const GURL& url);
  void UpdatePluginForNewGeometry(const gfx::Rect& window_rect,
                                  const gfx::Rect& unobscured_rect);

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner();

  // Manages its own lifetime.
  raw_ptr<Delegate> delegate_;

  ui::Cursor current_cursor_;

  // Owns us.
  raw_ptr<blink::WebPluginContainer> container_;

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
                        public blink::WebNonCompositedWidgetClient,
                        public blink::WebLocalFrameClient,
                        public blink::mojom::WidgetHost {
   public:
    WebViewHelper(
        WebViewPlugin* plugin,
        const blink::web_pref::WebPreferences& parent_web_preferences,
        const blink::RendererPreferences& parent_renderer_preferences);
    ~WebViewHelper() override;

    blink::WebView* web_view() { return web_view_; }
    blink::WebNavigationControl* main_frame() { return frame_; }

    // WebViewClient methods:
    void InvalidateContainer() override;

    // WebNonCompositedWidgetClient overrides.
    void ScheduleNonCompositedAnimation() override;

    // WebLocalFrameClient methods:
    void BindToFrame(blink::WebNavigationControl* frame) override;
    void DidClearWindowObject() override;
    void FrameDetached() override;
    scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
        override;

    // blink::mojom::WidgetHost implementation.
    void SetCursor(const ui::Cursor& cursor) override;
    void UpdateTooltipUnderCursor(const std::u16string& tooltip_text,
                                  base::i18n::TextDirection hint) override;
    void UpdateTooltipFromKeyboard(const std::u16string& tooltip_text,
                                   base::i18n::TextDirection hint,
                                   const gfx::Rect& bounds) override;
    void ClearKeyboardTriggeredTooltip() override;
    void TextInputStateChanged(ui::mojom::TextInputStatePtr state) override {}
    void SelectionBoundsChanged(const gfx::Rect& anchor_rect,
                                base::i18n::TextDirection anchor_dir,
                                const gfx::Rect& focus_rect,
                                base::i18n::TextDirection focus_dir,
                                const gfx::Rect& bounding_box,
                                bool is_anchor_first) override {}
    void CreateFrameSink(
        mojo::PendingReceiver<viz::mojom::CompositorFrameSink>
            compositor_frame_sink_receiver,
        mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient>) override {}
    void RegisterRenderFrameMetadataObserver(
        mojo::PendingReceiver<cc::mojom::RenderFrameMetadataObserverClient>
            render_frame_metadata_observer_client_receiver,
        mojo::PendingRemote<cc::mojom::RenderFrameMetadataObserver>
            render_frame_metadata_observer) override {}

    // This function sets the "title" attribute to the text value passed by
    // parameter on the container's element, if possible.
    void UpdateTooltip(const std::u16string& tooltip_text);

   private:
    raw_ptr<WebViewPlugin> plugin_;
    raw_ptr<blink::WebNavigationControl> frame_ = nullptr;

    std::unique_ptr<blink::scheduler::WebAgentGroupScheduler>
        agent_group_scheduler_;

    // Owned by us, deleted via |close()|.
    raw_ptr<blink::WebView, DanglingUntriaged> web_view_;

    mojo::AssociatedReceiver<blink::mojom::WidgetHost>
        blink_widget_host_receiver_{this};
    mojo::AssociatedRemote<blink::mojom::Widget> blink_widget_;
  };
  WebViewHelper web_view_helper_;

  // Should be invalidated when destroy() is called.
  base::WeakPtrFactory<WebViewPlugin> weak_factory_{this};
};

#endif  // COMPONENTS_PLUGINS_RENDERER_WEBVIEW_PLUGIN_H_
