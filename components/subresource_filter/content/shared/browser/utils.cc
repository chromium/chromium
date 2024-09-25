// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/shared/browser/utils.h"

#include "base/check.h"
#include "components/subresource_filter/content/shared/common/utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace subresource_filter {

bool ShouldInheritOpenerActivation(content::NavigationHandle* navigation_handle,
                                   content::RenderFrameHost* frame_host) {
  // TODO(bokan): Add and use GetOpener associated with `frame_host`'s Page.
  // https://crbug.com/1230153.
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return false;
  }

  content::RenderFrameHost* opener_rfh =
      navigation_handle->GetWebContents()->GetOpener();
  if (!opener_rfh) {
    return false;
  }

  if (!frame_host->GetLastCommittedOrigin().IsSameOriginWith(
          opener_rfh->GetLastCommittedOrigin())) {
    return false;
  }

  return !navigation_handle->HasCommitted() ||
         ShouldInheritActivation(navigation_handle->GetURL());
}

bool ShouldInheritParentActivation(
    content::NavigationHandle* navigation_handle) {
  // TODO(https://crbug.com/40202987): Investigate if this should apply to
  // fenced frames as well, or if we can default them to unactivated initially.
  if (navigation_handle->IsInMainFrame()) {
    return false;
  }
  CHECK(navigation_handle->GetParentFrame());

  // As with ShouldInheritSameOriginOpenerActivation() except that we inherit
  // from the parent frame as we are a subframe.
  return ShouldInheritActivation(navigation_handle->GetURL()) ||
         !navigation_handle->HasCommitted();
}

bool IsInSubresourceFilterRoot(content::NavigationHandle* navigation_handle) {
  switch (navigation_handle->GetNavigatingFrameType()) {
    case content::FrameType::kPrimaryMainFrame:
    case content::FrameType::kPrerenderMainFrame:
      return true;
    case content::FrameType::kSubframe:
    case content::FrameType::kFencedFrameRoot:
      return false;
  }
}

bool IsSubresourceFilterRoot(content::RenderFrameHost* rfh) {
  return !rfh->GetParent() && !rfh->IsFencedFrameRoot();
}

content::Page& GetSubresourceFilterRootPage(content::RenderFrameHost* rfh) {
  CHECK(rfh);
  // If we ever add a new embedded page type (we only have fenced frames
  // currently), we should reconsider if we should escape its page boundary
  // here.
  CHECK(!rfh->GetMainFrame()->GetParentOrOuterDocument() ||
        rfh->IsNestedWithinFencedFrame());
  return rfh->GetOutermostMainFrame()->GetPage();
}

}  // namespace subresource_filter
