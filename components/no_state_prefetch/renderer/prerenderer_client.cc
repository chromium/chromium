// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/no_state_prefetch/renderer/prerenderer_client.h"

#include "base/logging.h"
#include "components/no_state_prefetch/renderer/prerender_helper.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "third_party/blink/public/web/web_view.h"

namespace prerender {

PrerendererClient::PrerendererClient(content::RenderView* render_view)
    : content::RenderViewObserver(render_view) {
  DCHECK(render_view);
  DVLOG(5) << "PrerendererClient::PrerendererClient()";
  render_view->GetWebView()->SetPrerendererClient(this);
}

PrerendererClient::~PrerendererClient() = default;

bool PrerendererClient::IsPrefetchOnly() {
  return PrerenderHelper::GetPrerenderMode(
             render_view()->GetMainRenderFrame()) ==
         prerender::mojom::PrerenderMode::kPrefetchOnly;
}

void PrerendererClient::OnDestruct() {
  delete this;
}

}  // namespace prerender
