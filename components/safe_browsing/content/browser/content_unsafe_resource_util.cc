// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/content_unsafe_resource_util.h"

#include "components/safe_browsing/content/browser/async_check_tracker.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace safe_browsing::unsafe_resource_util {

content::NavigationEntry* GetNavigationEntryForResource(
    const security_interstitials::UnsafeResource& resource) {
  return GetNavigationEntryForLocator(
      resource.rfh_locator, resource.navigation_id, resource.threat_type);
}

content::NavigationEntry* GetNavigationEntryForLocator(
    const security_interstitials::UnsafeResourceLocator& rfh_locator,
    const std::optional<int64_t>& navigation_id,
    safe_browsing::SBThreatType threat_type) {
  content::WebContents* web_contents =
      unsafe_resource_util::GetWebContentsForLocator(rfh_locator);
  if (!web_contents) {
    return nullptr;
  }
  // If a safebrowsing hit occurs during main frame navigation, the navigation
  // will not be committed, and the pending navigation entry refers to the hit.
  if (AsyncCheckTracker::IsMainPageLoadPending(rfh_locator, navigation_id,
                                               threat_type)) {
    return web_contents->GetController().GetPendingEntry();
  }
  // If a safebrowsing hit occurs on a subresource load, or on a main frame
  // after the navigation is committed, the last committed navigation entry
  // refers to the page with the hit. Note that there may concurrently be an
  // unrelated pending navigation to another site, so GetActiveEntry() would be
  // wrong.
  return web_contents->GetController().GetLastCommittedEntry();
}

content::WebContents* GetWebContentsForResource(
    const security_interstitials::UnsafeResource& resource) {
  return GetWebContentsForLocator(resource.rfh_locator);
}

content::WebContents* GetWebContentsForLocator(
    const security_interstitials::UnsafeResourceLocator& rfh_locator) {
  if (rfh_locator.render_frame_token) {
    content::RenderFrameHost* rfh = content::RenderFrameHost::FromFrameToken(
        content::GlobalRenderFrameHostToken(
            rfh_locator.render_process_id,
            blink::LocalFrameToken(rfh_locator.render_frame_token.value())));
    if (rfh) {
      return content::WebContents::FromRenderFrameHost(rfh);
    }
  }
  return content::WebContents::FromFrameTreeNodeId(
      content::FrameTreeNodeId(rfh_locator.frame_tree_node_id));
}

}  // namespace safe_browsing::unsafe_resource_util
