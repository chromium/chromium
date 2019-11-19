// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/content/renderer/overlay_js_render_frame_observer.h"

#include <utility>

#include "base/bind.h"
#include "components/contextual_search/content/renderer/contextual_search_wrapper.h"
#include "content/public/renderer/render_frame.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_document.h"
#include "v8/include/v8.h"

namespace contextual_search {

OverlayJsRenderFrameObserver::OverlayJsRenderFrameObserver(
    content::RenderFrame* render_frame,
    service_manager::BinderRegistry* registry)
    : RenderFrameObserver(render_frame) {}

OverlayJsRenderFrameObserver::~OverlayJsRenderFrameObserver() {}

void OverlayJsRenderFrameObserver::DidClearWindowObject() {
  if (!did_start_enabling_js_api_) {
    blink::WebURL url = render_frame()->GetWebFrame()->GetDocument().Url();
    GURL gurl(url);
    if (!url.IsEmpty() && EnsureServiceConnected()) {
      did_start_enabling_js_api_ = true;
      contextual_search_js_api_service_->ShouldEnableJsApi(
          gurl, base::BindOnce(&OverlayJsRenderFrameObserver::EnableJsApi,
                               weak_factory_.GetWeakPtr()));
    }
  }
}

void OverlayJsRenderFrameObserver::EnableJsApi(bool should_enable) {
  if (!should_enable)
    return;
  contextual_search::ContextualSearchWrapper::Install(render_frame());
}

bool OverlayJsRenderFrameObserver::EnsureServiceConnected() {
  if (render_frame() && !contextual_search_js_api_service_) {
    render_frame()->GetRemoteInterfaces()->GetInterface(
        contextual_search_js_api_service_.BindNewPipeAndPassReceiver());
    return true;
  }
  return false;
}

void OverlayJsRenderFrameObserver::OnDestruct() {
  delete this;
}

}  // namespace contextual_search
