// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/unsafe_resource_util.h"

#include "base/bind.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace security_interstitials {

content::NavigationEntry* GetNavigationEntryForResource(
    const UnsafeResource& resource) {
  content::WebContents* web_contents = resource.web_contents_getter.Run();
  if (!web_contents)
    return nullptr;
  // If a safebrowsing hit occurs during main frame navigation, the navigation
  // will not be committed, and the pending navigation entry refers to the hit.
  if (resource.IsMainPageLoadBlocked())
    return web_contents->GetController().GetPendingEntry();
  // If a safebrowsing hit occurs on a subresource load, or on a main frame
  // after the navigation is committed, the last committed navigation entry
  // refers to the page with the hit. Note that there may concurrently be an
  // unrelated pending navigation to another site, so GetActiveEntry() would be
  // wrong.
  return web_contents->GetController().GetLastCommittedEntry();
}

base::RepeatingCallback<content::WebContents*(void)> GetWebContentsGetter(
    int render_process_host_id,
    int render_frame_id) {
  return GetWebContentsGetter(content::GlobalRenderFrameHostId(
      render_process_host_id, render_frame_id));
}

base::RepeatingCallback<content::WebContents*(void)> GetWebContentsGetter(
    content::GlobalRenderFrameHostId render_frame_host_id) {
  return base::BindRepeating(
      [](content::GlobalRenderFrameHostId render_frame_host_id) {
        return content::WebContents::FromRenderFrameHost(
            content::RenderFrameHost::FromID(render_frame_host_id));
      },
      render_frame_host_id);
}

}  // namespace security_interstitials
