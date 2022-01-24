// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/previews/resource_loading_hints_agent.h"

#include <vector>

#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace previews {


ResourceLoadingHintsAgent::ResourceLoadingHintsAgent(
    blink::AssociatedInterfaceRegistry* associated_interfaces,
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {
  DCHECK(render_frame);
  associated_interfaces->AddInterface(base::BindRepeating(
      &ResourceLoadingHintsAgent::SetReceiver, base::Unretained(this)));
}

GURL ResourceLoadingHintsAgent::GetDocumentURL() const {
  return render_frame()->GetWebFrame()->GetDocument().Url();
}

void ResourceLoadingHintsAgent::OnDestruct() {
  delete this;
}

ResourceLoadingHintsAgent::~ResourceLoadingHintsAgent() = default;

void ResourceLoadingHintsAgent::SetReceiver(
    mojo::PendingAssociatedReceiver<
        previews::mojom::PreviewsResourceLoadingHintsReceiver> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

bool ResourceLoadingHintsAgent::IsMainFrame() const {
  return render_frame()->IsMainFrame();
}


void ResourceLoadingHintsAgent::SetLiteVideoHint(
    previews::mojom::LiteVideoHintPtr lite_video_hint) {
  auto* lite_video_hint_agent =
      lite_video::LiteVideoHintAgent::Get(render_frame());
  if (lite_video_hint_agent)
    lite_video_hint_agent->SetLiteVideoHint(std::move(lite_video_hint));
}

void ResourceLoadingHintsAgent::StopThrottlingMediaRequests() {
  auto* lite_video_hint_agent =
      lite_video::LiteVideoHintAgent::Get(render_frame());
  if (lite_video_hint_agent) {
    LOCAL_HISTOGRAM_BOOLEAN("LiteVideo.HintsAgent.StopThrottling", true);
    lite_video_hint_agent->StopThrottlingAndClearHints();
  }
}

}  // namespace previews
