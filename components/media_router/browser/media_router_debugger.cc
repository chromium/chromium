// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/media_router_debugger.h"

#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_factory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_switches.h"

namespace media_router {

MediaRouterDebugger::MediaRouterDebugger() = default;
MediaRouterDebugger::~MediaRouterDebugger() = default;

// static.
MediaRouterDebugger* MediaRouterDebugger::GetForFrameTreeNode(
    int frame_tree_node_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (!web_contents) {
    return nullptr;
  }

  auto* media_router = MediaRouterFactory::GetApiForBrowserContextIfExists(
      web_contents->GetBrowserContext());

  return media_router ? &media_router->GetDebugger() : nullptr;
}

void MediaRouterDebugger::EnableRtcpReports() {
  is_rtcp_reports_enabled_ = true;
}

void MediaRouterDebugger::DisableRtcpReports() {
  is_rtcp_reports_enabled_ = false;
}

bool MediaRouterDebugger::IsRtcpReportsEnabled() const {
  return is_rtcp_reports_enabled_ &&
         base::FeatureList::IsEnabled(media::kEnableRtcpReporting);
}

void MediaRouterDebugger::AddObserver(MirroringStatsObserver& obs) {
  observers_.AddObserver(&obs);
}

void MediaRouterDebugger::RemoveObserver(MirroringStatsObserver& obs) {
  observers_.RemoveObserver(&obs);
}

}  // namespace media_router
