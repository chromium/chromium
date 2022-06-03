// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_WEBVIEW_WEB_CONTENT_CONTROLLER_H_
#define CHROMECAST_BROWSER_WEBVIEW_WEB_CONTENT_CONTROLLER_H_

#include <deque>
#include <memory>
#include <set>
#include <string>

#include "chromecast/browser/webview/js_channel_service.h"
#include "chromecast/browser/webview/proto/webview.pb.h"
#include "components/exo/surface.h"
#include "components/exo/surface_observer.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/events/gestures/gesture_recognizer_impl.h"

namespace aura {
class Window;
}  // namespace aura

namespace content {
class RenderFrameHost;
}  // namespace content

namespace chromecast {

class WebContentJsChannels;

// Processes proto commands to control WebContents
class WebContentController
    : public exo::SurfaceObserver,
      public content::WebContentsObserver,
      public content::RenderWidgetHost::InputEventObserver,
      public JsClientInstance::Observer {
 public:
  class Client {
   public:
    virtual ~Client() {}
    virtual void EnqueueSend(
        std::unique_ptr<webview::WebviewResponse> response) = 0;
    virtual void OnError(const std::string& error_message) = 0;
  };
  // Touch event information recorded so that acks can be sent in the same
  // order. Stripped down from the normal event flow's TouchEventAckQueue.
  struct TouchData {
    uint32_t id;
    content::RenderWidgetHostView* rwhv;
    bool acked;
    ui::EventResult result;
  };

  explicit WebContentController(Client* client);

  WebContentController(const WebContentController&) = delete;
  WebContentController& operator=(const WebContentController&) = delete;

  ~WebContentController() override;

  virtual void Destroy() = 0;

  virtual void ProcessRequest(const webview::WebviewRequest& request);

  // Attach this web contents to an aura window as a child.
  void AttachTo(aura::Window* window, int window_id);

  // Invoked when the aura window becomes visible and is fully initialized.
  void OnVisible(aura::Window* window);

  Client* client() const { return client_; }

 protected:
  // content::WebContentsObserver
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;
  void PrimaryMainFrameWasResized(bool width_changed) override;
  void FrameSizeChanged(content::RenderFrameHost* render_frame_host,
                        const gfx::Size& frame_size) override;

  static void RegisterRenderWidgetInputObserverFromRenderFrameHost(
      WebContentController* web_content_controller,
      content::RenderFrameHost* render_frame_host);

  // Subclasses are expected to add/remove this as a WebContentsObserver on
  // whatever WebContents this manages.
  virtual content::WebContents* GetWebContents() = 0;
  Client* client_;  // Not owned.
  bool has_navigation_delegate_ = false;
  std::unique_ptr<WebContentJsChannels> js_channels_;

 private:
  void ProcessInputEvent(const webview::InputEvent& ev);
  void RegisterRenderWidgetInputObserver(
      content::RenderWidgetHost* render_widget_host);
  void UnregisterRenderWidgetInputObserver(
      content::RenderWidgetHost* render_widget_host);
  void AckTouchEvent(content::RenderWidgetHostView* rhwv,
                     uint32_t unique_event_id,
                     ui::EventResult result);
  void JavascriptCallback(int64_t id, base::Value result);
  void HandleEvaluateJavascript(
      int64_t id,
      const webview::EvaluateJavascriptRequest& request);
  void HandleAddJavascriptChannels(
      const webview::AddJavascriptChannelsRequest& request);
  void HandleRemoveJavascriptChannels(
      const webview::RemoveJavascriptChannelsRequest& request);
  void HandleGetCurrentUrl(int64_t id);
  void HandleCanGoBack(int64_t id);
  void HandleCanGoForward(int64_t id);
  void HandleClearCache();
  void HandleClearCookies(int64_t id);
  void HandleGetTitle(int64_t id);
  void HandleResize(const gfx::Size& size);
  void HandleSetInsets(const gfx::Insets& insets);
  void HandleGetUserAgent(int64_t id);

  viz::SurfaceId GetSurfaceId();
  void ChannelModified(content::RenderFrameHost* frame,
                       const std::string& channel,
                       bool added);
  JsChannelCallback GetJsChannelCallback();
  void SendInitialChannelSet(JsClientInstance* instance);

  // exo::SurfaceObserver
  void OnSurfaceDestroying(exo::Surface* surface) override;

  // JsClientInstance::Observer
  void OnJsClientInstanceRegistered(int process_id,
                                    int routing_id,
                                    JsClientInstance* instance) override;

  // content::RenderWidgetHost::InputEventObserver
  void OnInputEventAck(blink::mojom::InputEventResultSource source,
                       blink::mojom::InputEventResultState state,
                       const blink::WebInputEvent&) override;

  ui::GestureRecognizerImpl gesture_recognizer_;
  std::deque<TouchData> touch_queue_;

  exo::Surface* surface_ = nullptr;

  std::set<std::string> current_javascript_channel_set_;
  std::set<content::RenderFrameHost*> current_render_frame_set_;
  std::set<content::RenderWidgetHost*> current_render_widget_set_;

  // Observes the aura window and calls back to the controller for visibility
  // events.
  class WebviewWindowVisibilityObserver : public aura::WindowObserver {
   public:
    explicit WebviewWindowVisibilityObserver(aura::Window* window,
                                             WebContentController* controller);
    ~WebviewWindowVisibilityObserver() override;

    WebviewWindowVisibilityObserver(const WebviewWindowVisibilityObserver&) =
        delete;
    WebviewWindowVisibilityObserver& operator=(
        const WebviewWindowVisibilityObserver&) = delete;

   private:
    // aura::WindowObserver
    void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;
    void OnWindowDestroyed(aura::Window* window) override;

    aura::Window* window_;
    WebContentController* controller_;
  };

  std::unique_ptr<WebviewWindowVisibilityObserver> window_visibility_observer_;
  std::unique_ptr<ui::InputMethodObserver> input_method_observer_;

  base::WeakPtrFactory<WebContentController> weak_ptr_factory_{this};
};

class WebContentJsChannels
    : public base::SupportsWeakPtr<WebContentJsChannels> {
 public:
  explicit WebContentJsChannels(WebContentController::Client* client);

  WebContentJsChannels(const WebContentJsChannels&) = delete;
  WebContentJsChannels& operator=(const WebContentJsChannels&) = delete;

  ~WebContentJsChannels();

  void SendMessage(const std::string& channel, const std::string& message);

 private:
  WebContentController::Client* client_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_WEBVIEW_WEB_CONTENT_CONTROLLER_H_
