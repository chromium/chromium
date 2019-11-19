// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_WEBVIEW_WEB_CONTENT_CONTROLLER_H_
#define CHROMECAST_BROWSER_WEBVIEW_WEB_CONTENT_CONTROLLER_H_

#include <memory>
#include <set>
#include <string>

#include "chromecast/browser/webview/js_channel_service.h"
#include "chromecast/browser/webview/proto/webview.pb.h"
#include "components/exo/surface.h"
#include "components/exo/surface_observer.h"
#include "content/public/browser/web_contents_observer.h"
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
class WebContentController : public exo::SurfaceObserver,
                             public content::WebContentsObserver,
                             public JsClientInstance::Observer {
 public:
  class Client {
   public:
    virtual ~Client() {}
    virtual void EnqueueSend(
        std::unique_ptr<webview::WebviewResponse> response) = 0;
    virtual void OnError(const std::string& error_message) = 0;
  };
  WebContentController(Client* client);
  ~WebContentController() override;

  virtual void Destroy() = 0;

  virtual void ProcessRequest(const webview::WebviewRequest& request);

  // Attach this web contents to an aura window as a child.
  void AttachTo(aura::Window* window, int window_id);

 protected:
  // Subclasses are expected to add/remove this as a WebContentsObserver on
  // whatever WebContents this manages.
  virtual content::WebContents* GetWebContents() = 0;
  Client* client_;  // Not owned.
  bool has_navigation_delegate_ = false;
  std::unique_ptr<WebContentJsChannels> js_channels_;

 private:
  void ProcessInputEvent(const webview::InputEvent& ev);
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
  void HandleGetTitle(int64_t id);
  void HandleUpdateSettings(const webview::UpdateSettingsRequest& request);
  void HandleSetAutoMediaPlaybackPolicy(
      const webview::SetAutoMediaPlaybackPolicyRequest& request);
  viz::SurfaceId GetSurfaceId();
  void ChannelModified(content::RenderFrameHost* frame,
                       const std::string& channel,
                       bool added);
  JsChannelCallback GetJsChannelCallback();
  void SendInitialChannelSet(JsClientInstance* instance);

  // exo::SurfaceObserver
  void OnSurfaceDestroying(exo::Surface* surface) override;

  // content::WebContentsObserver
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;

  // JsClientInstance::Observer
  void OnJsClientInstanceRegistered(int process_id,
                                    int routing_id,
                                    JsClientInstance* instance) override;

  ui::GestureRecognizerImpl gesture_recognizer_;

  exo::Surface* surface_ = nullptr;

  std::set<std::string> current_javascript_channel_set_;
  std::set<content::RenderFrameHost*> current_render_frame_set_;

  base::WeakPtrFactory<WebContentController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebContentController);
};

class WebContentJsChannels
    : public base::SupportsWeakPtr<WebContentJsChannels> {
 public:
  explicit WebContentJsChannels(WebContentController::Client* client);
  ~WebContentJsChannels();

  void SendMessage(const std::string& channel, const std::string& message);

 private:
  WebContentController::Client* client_;

  DISALLOW_COPY_AND_ASSIGN(WebContentJsChannels);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_WEBVIEW_WEB_CONTENT_CONTROLLER_H_
