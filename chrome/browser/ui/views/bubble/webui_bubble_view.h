// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BUBBLE_WEBUI_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_BUBBLE_WEBUI_BUBBLE_VIEW_H_

#include <memory>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"

namespace content {
class BrowserContext;
}  // namespace content

// WebUIBubbleView is used to host WebUI that notifies its host when rendering
// has finished by calling Host::ShowUI(). Similarly WebUIBubbleView notifies
// its host of WebUI size changes by calling Host::OnWebViewSizeChanged().
class WebUIBubbleView : public views::WebView,
                        public ui::MojoBubbleWebUIController::Embedder {
 public:
  class Host {
   public:
    virtual void ShowUI() = 0;
    virtual void CloseUI() = 0;
    virtual void OnWebViewSizeChanged() = 0;
  };

  explicit WebUIBubbleView(content::BrowserContext* browser_context);
  ~WebUIBubbleView() override;

  // The type T enables WebUIBubbleView to know what WebUIController is being
  // used for the hosted WebUI and allows it to make sure the associated WebUI
  // is a MojoBubbleWebUIController at compile time.
  template <typename T>
  void LoadURL(const GURL& url) {
    // Lie to WebContents so it starts rendering and eventually calls ShowUI().
    GetWebContents()->WasShown();
    SetVisible(true);
    LoadInitialURL(url);
    T* async_webui_controller =
        GetWebContents()->GetWebUI()->GetController()->template GetAs<T>();
    // Depends on the WebUIController object being constructed synchronously
    // when the navigation is started in LoadInitialURL().
    async_webui_controller->set_embedder(weak_ptr_factory_.GetWeakPtr());
  }

  void set_host(Host* host) { host_ = host; }

  // WebView:
  void PreferredSizeChanged() override;
  bool HandleContextMenu(content::RenderFrameHost* render_frame_host,
                         const content::ContextMenuParams& params) override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;

  // MojoBubbleWebUIController::Embedder:
  void ShowUI() override;
  void CloseUI() override;

 private:
  // |host_| does not always have to be set. The WebUIBubbleView can be cached
  // and running in the background without a host being present.
  Host* host_ = nullptr;

  // A handler to handle unhandled keyboard messages coming back from the
  // renderer process.
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  base::WeakPtrFactory<WebUIBubbleView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_BUBBLE_WEBUI_BUBBLE_VIEW_H_
