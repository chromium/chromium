// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_distiller/content/renderer/distiller_js_render_frame_observer.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "components/web_distiller/core/url_constants.h"
#include "components/web_distiller/core/url_utils.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/v8.h"

namespace web_distiller {

DistillerJsRenderFrameObserver::DistillerJsRenderFrameObserver(
    content::RenderFrame* render_frame,
    const int32_t distiller_isolated_world_id)
    : RenderFrameObserver(render_frame),
      distiller_isolated_world_id_(distiller_isolated_world_id),
      is_distiller_page_(false) {}

DistillerJsRenderFrameObserver::~DistillerJsRenderFrameObserver() = default;

void DistillerJsRenderFrameObserver::DidStartNavigation(
    const GURL& url,
    std::optional<blink::WebNavigationType> navigation_type) {
  is_distiller_page_ =
      render_frame()->IsMainFrame() && url_utils::IsDistilledPage(url);
}

void DistillerJsRenderFrameObserver::DidCreateScriptContext(
    v8::Local<v8::Context> context,
    int32_t world_id) {
  if (world_id != distiller_isolated_world_id_ || !is_distiller_page_ ||
      !render_frame()->IsMainFrame()) {
    return;
  }
  const blink::WebDocument document =
      render_frame()->GetWebFrame()->GetDocument();
  if (!document.Url().ProtocolIs(kWebDistillerScheme) ||
      document.GetSecurityOrigin().Protocol() != kWebDistillerScheme) {
    return;
  }

  native_javascript_handle_ =
      std::make_unique<DistillerNativeJavaScript>(render_frame());
  native_javascript_handle_->AddJavaScriptObjectToFrame(context);
}

void DistillerJsRenderFrameObserver::OnDestruct() {
  delete this;
}

}  // namespace web_distiller
