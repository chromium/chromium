// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_WEBVIEW_CAST_CONTENT_WINDOW_EMBEDDED_H_
#define CHROMECAST_BROWSER_WEBVIEW_CAST_CONTENT_WINDOW_EMBEDDED_H_

#include <string>

#include "chromecast/browser/cast_content_window.h"
#include "chromecast/browser/webview/cast_window_embedder.h"
#include "chromecast/ui/back_gesture_router.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace content {
class WebContents;
}  // namespace content

namespace chromecast {
class CastWebContents;

// Embedded Cast window implementation to work with webview window
// management system. This class will receive embedder's window events
// and reports its Cast window specific request to embedder.
// An instance of this object is created for each CastWebContents instance. Its
// web contents may be destroyed before |this| is destroyed, so
// WebContentsObserver is used to observe the web contents to prevent garbage
// from being returned in GetWebContents.
class CastContentWindowEmbedded
    : public CastContentWindow,
      public aura::WindowObserver,
      public content::WebContentsObserver,
      public chromecast::BackGestureRouter::Delegate,
      public CastWindowEmbedder::EmbeddedWindow {
 public:
  // |cast_window_embedder|: The corresponding embedder that this window
  // listens on incoming window events. Must outlive |this|.
  // |force_720p_resolution|: Whether 720p resolution is enabled/forced for
  // this window's hosted web page (i.e. a CastWebView).
  CastContentWindowEmbedded(mojom::CastWebViewParamsPtr params,
                            CastWindowEmbedder* cast_window_embedder,
                            bool force_720p_resolution);
  ~CastContentWindowEmbedded() override;
  CastContentWindowEmbedded(const CastContentWindowEmbedded&) = delete;
  CastContentWindowEmbedded& operator=(const CastContentWindowEmbedded&) =
      delete;

  // CastContentWindow implementation:
  void CreateWindow(::chromecast::mojom::ZOrder z_order,
                    VisibilityPriority visibility_priority) override;
  void GrantScreenAccess() override;
  void RevokeScreenAccess() override;
  void RequestVisibility(VisibilityPriority visibility_priority) override;
  void SetActivityContext(base::Value activity_context) override;
  void SetHostContext(base::Value host_context) override;
  void RequestMoveOut() override;
  void EnableTouchInput(bool enabled) override;

  // aura::WindowObserver implementation:
  void OnWindowDestroyed(aura::Window* window) override;

  // CastWindowEmbedder::EmbeddedWindow implementation:
  int GetWindowId() override;
  std::string GetAppId() override;
  void OnEmbedderWindowEvent(
      const CastWindowEmbedder::EmbedderWindowEvent& request) override;
  content::WebContents* GetWebContents() override;
  CastWebContents* GetCastWebContents() override;
  void DispatchState() override;
  void SendAppContext(const std::string& context) override;
  void Stop() override;

  // chromecast::GestureRouter::Delegate
  void SetCanGoBack(bool can_go_back) override;

  void RegisterBackGestureRouter(
      ::chromecast::BackGestureRouter* back_gesture_router) override;

 private:
  // Sending a window request to the embedder to handle.
  // |request_type| specifies the type of the request, e.g. creation of new
  // window, request of focus, and so forth.
  void SendWindowRequest(CastWindowEmbedder::WindowRequestType request_type);

  // Collects and generates a |CastWindowProperties| to represent the current
  // state of this window, in terms of the embedder needs to know about.
  CastWindowEmbedder::CastWindowProperties PopulateCastWindowProperties();

  void ReleaseFocus();
  void RequestFocus();

  // Sends open window request only when this window has gained screen access
  // and has not reported the creation/open of this window to the embedder yet.
  void MaybeSendOpenWindowRequest();

  // Sends the current CastWindowProperties to |cast_window_embedder_|.
  void SendCastWindowProperties();

  void SendOpenWindowRequest();

  void ConsumeGestureCompleted(bool handled);

  // content::WebContentsObserver
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DidFirstVisuallyNonEmptyPaint() override;
  void WebContentsDestroyed() override;

  // Once set, those window properties are fixed during lifetime of |this|.
  std::string app_id_;
  std::string session_id_;
  int window_id_ = -1;
  bool is_remote_control_ = false;
  CastWindowEmbedder* cast_window_embedder_ = nullptr;
  const bool force_720p_resolution_ = false;

  // States might change during lifetime of |this|.
  bool open_window_sent_ = false;
  bool can_go_back_ = false;
  bool has_screen_access_ = false;
  VisibilityPriority visibility_priority_ = VisibilityPriority::DEFAULT;
  aura::Window* window_ = nullptr;
  CastWebContents* cast_web_contents_ = nullptr;
  base::Value activity_context_;

  // A free-form custom data field for communicating with the window embedder.
  base::Value host_context_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_WEBVIEW_CAST_CONTENT_WINDOW_EMBEDDED_H_
