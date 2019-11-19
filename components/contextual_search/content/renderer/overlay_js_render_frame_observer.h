// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_CONTENT_RENDERER_OVERLAY_JS_RENDER_FRAME_OBSERVER_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_CONTENT_RENDERER_OVERLAY_JS_RENDER_FRAME_OBSERVER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/contextual_search/content/common/mojom/contextual_search_js_api_service.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/v8.h"

namespace contextual_search {

// OverlayJsRenderFrame observer waits for a page to be loaded and then
// tries to connect to a mojo service hosted in the browser process. The
// service will tell this render process if the current page is presented
// in an overlay panel.
class OverlayJsRenderFrameObserver : public content::RenderFrameObserver {
 public:
  OverlayJsRenderFrameObserver(content::RenderFrame* render_frame,
                               service_manager::BinderRegistry* registry);
  ~OverlayJsRenderFrameObserver() override;

  // RenderFrameObserver implementation.
  void DidClearWindowObject() override;

 private:
  // RenderFrameObserver implementation.
  void OnDestruct() override;

  // Helper function to ensure that this class has connected to the CS service.
  // Returns false if cannot connect.
  bool EnsureServiceConnected();

  // Enables or disables the JS API.
  void EnableJsApi(bool should_enable);

  // The CS service to notify when deciding to enable the API or when API calls
  // are made.
  mojo::Remote<mojom::ContextualSearchJsApiService>
      contextual_search_js_api_service_;

  // Remembers whether we did start enabling the JS API by making a request
  // to the Contextual Search service to ask if we should enable for this
  // URL or not.
  bool did_start_enabling_js_api_ = false;

  base::WeakPtrFactory<OverlayJsRenderFrameObserver> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(OverlayJsRenderFrameObserver);
};

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_CONTENT_RENDERER_OVERLAY_JS_RENDER_FRAME_OBSERVER_H_
