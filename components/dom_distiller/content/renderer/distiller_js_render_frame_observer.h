// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CONTENT_RENDERER_DISTILLER_JS_RENDER_FRAME_OBSERVER_H_
#define COMPONENTS_DOM_DISTILLER_CONTENT_RENDERER_DISTILLER_JS_RENDER_FRAME_OBSERVER_H_

#include "base/memory/weak_ptr.h"
#include "components/dom_distiller/content/common/mojom/distiller_javascript_service.mojom.h"
#include "components/dom_distiller/content/renderer/distiller_native_javascript.h"
#include "components/dom_distiller/content/renderer/distiller_page_notifier_service_impl.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "v8/include/v8.h"

namespace dom_distiller {

// DistillerJsRenderFrame observer waits for a page to be loaded and then
// tried to connect to a mojo service hosted in the browser process. The
// service will tell this render process if the current page is a distiller
// page.
class DistillerJsRenderFrameObserver : public content::RenderFrameObserver {
 public:
  DistillerJsRenderFrameObserver(content::RenderFrame* render_frame,
                                 const int32_t distiller_isolated_world_id,
                                 service_manager::BinderRegistry* registry);
  ~DistillerJsRenderFrameObserver() override;

  // RenderFrameObserver implementation.
  void DidStartNavigation(
      const GURL& url,
      base::Optional<blink::WebNavigationType> navigation_type) override;
  void DidFinishLoad() override;
  void DidCreateScriptContext(v8::Local<v8::Context> context,
                              int32_t world_id) override;

  // Flag the current page as a distiller page.
  void SetIsDistillerPage();

 private:
  void CreateDistillerPageNotifierService(
      mojo::PendingReceiver<mojom::DistillerPageNotifierService> receiver);

  // RenderFrameObserver implementation.
  void OnDestruct() override;

  // The isolated world that the distiller object should be written to.
  int32_t distiller_isolated_world_id_;

  // Track if the current page is distilled. This is needed for testing.
  bool is_distiller_page_;

  // True if a load is in progress and we are currently able to bind requests
  // for mojom::DistillerPageNotifierService.
  bool load_active_ = false;

  // Handle to "distiller" JavaScript object functionality.
  std::unique_ptr<DistillerNativeJavaScript> native_javascript_handle_;
  base::WeakPtrFactory<DistillerJsRenderFrameObserver> weak_factory_{this};
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CONTENT_RENDERER_DISTILLER_JS_RENDER_FRAME_OBSERVER_H_
