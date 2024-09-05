// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/mirroring_to_flinging_switcher.h"

#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/browser/presentation/web_contents_presentation_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/presentation_request.h"
#include "content/public/browser/web_contents.h"

namespace media_router {

void SwitchToFlingingIfPossible(content::FrameTreeNodeId frame_tree_node_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (!web_contents) {
    return;
  }

  base::WeakPtr<WebContentsPresentationManager>
      web_contents_presentation_manager =
          WebContentsPresentationManager::Get(web_contents);
  if (!web_contents_presentation_manager ||
      !web_contents_presentation_manager->HasDefaultPresentationRequest()) {
    return;
  }

  auto* media_router = MediaRouterFactory::GetApiForBrowserContextIfExists(
      web_contents->GetBrowserContext());
  if (!media_router) {
    return;
  }

  // TODO(crbug.com/1418744): Handle multiple URLs.
  const auto& presentation_request =
      web_contents_presentation_manager->GetDefaultPresentationRequest();
  const auto source_id =
      MediaSource::ForPresentationUrl(presentation_request.presentation_urls[0])
          .id();
  media_router->JoinRoute(
      source_id, kAutoJoinPresentationId, presentation_request.frame_origin,
      web_contents,
      base::BindOnce(&WebContentsPresentationManager::OnPresentationResponse,
                     std::move(web_contents_presentation_manager),
                     presentation_request),
      base::TimeDelta());
}

}  // namespace media_router
