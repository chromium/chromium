// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CONTENT_RENDERER_DISTILLER_JS_RENDER_FRAME_OBSERVER_H_
#define COMPONENTS_DOM_DISTILLER_CONTENT_RENDERER_DISTILLER_JS_RENDER_FRAME_OBSERVER_H_

#include "components/dom_distiller/content/renderer/distiller_native_javascript.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "v8/include/v8.h"

namespace dom_distiller {

// DistillerJsRenderFrame observer waits for a page to be loaded and then
// adds the Javascript distiller object if it is a distilled page.
class DistillerJsRenderFrameObserver : public content::RenderFrameObserver {
 public:
  DistillerJsRenderFrameObserver(content::RenderFrame* render_frame,
                                 const int32_t distiller_isolated_world_id);
  ~DistillerJsRenderFrameObserver() override;

  // RenderFrameObserver implementation.
  void DidStartNavigation(
      const GURL& url,
      std::optional<blink::WebNavigationType> navigation_type) override;
  void DidCreateScriptContext(v8::Local<v8::Context> context,
                              int32_t world_id) override;

 private:
  // RenderFrameObserver implementation.
  void OnDestruct() override;

  // The isolated world that the distiller object should be written to.
  int32_t distiller_isolated_world_id_;

  // Track if the current page is distilled. This is needed for testing.
  bool is_distiller_page_;

  // Handle to "distiller" JavaScript object functionality.
  std::unique_ptr<DistillerNativeJavaScript> native_javascript_handle_;
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CONTENT_RENDERER_DISTILLER_JS_RENDER_FRAME_OBSERVER_H_
