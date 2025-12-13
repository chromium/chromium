// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_WEBUI_BROWSER_WEBUI_BROWSER_RENDERER_EXTENSION_H_
#define CHROME_RENDERER_WEBUI_BROWSER_WEBUI_BROWSER_RENDERER_EXTENSION_H_

#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/gurl.h"
#include "v8/include/v8-forward.h"

// This class adds the chrome.browser API to chrome://webui-browser.
// The API has two methods:
// - allowCustomElementRegistration(callback): Allows the page to register
//   custom elements with arbitrary names. For example, names without a hyphen
//   ("-") can be used.
// - attachIframeGuest(guestContentsId, contentWindow): Attaches a guest
//   contents to an iframe on the page.
class WebUIBrowserRendererExtension : public content::RenderFrameObserver {
 public:
  // Creates a new instance, with ownership transferred to the RenderFrame.
  static void Create(content::RenderFrame* frame);

  WebUIBrowserRendererExtension(const WebUIBrowserRendererExtension&) = delete;
  WebUIBrowserRendererExtension& operator=(
      const WebUIBrowserRendererExtension&) = delete;

  ~WebUIBrowserRendererExtension() override;

  // content::RenderFrameObserver:
  void OnDestruct() override;
  void DidCreateScriptContext(v8::Local<v8::Context> v8_context,
                              int32_t world_id) override;

 private:
  explicit WebUIBrowserRendererExtension(content::RenderFrame* frame);

  void InjectScript();

  base::WeakPtrFactory<WebUIBrowserRendererExtension> weak_ptr_factory_{this};
};

#endif  // CHROME_RENDERER_WEBUI_BROWSER_WEBUI_BROWSER_RENDERER_EXTENSION_H_
