// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/shared/common/subresource_filter_utils.h"

#include "base/check.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/url_utils.h"
#include "url/gurl.h"

namespace subresource_filter {

bool ShouldInheritActivation(const GURL& url) {
  return !content::IsURLHandledByNetworkStack(url);
}

bool IsInSubresourceFilterRoot(content::NavigationHandle* navigation_handle) {
  // TODO(bokan): This should eventually consider Portals. crbug.com/1267506.
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
  // This only "breaks out" from fenced frames since the desired behavior in
  // other nested frame trees (e.g. portals) isn't clear. Otherwise we could
  // just use GetOutermostMainFrame.
  while (rfh->IsNestedWithinFencedFrame()) {
    rfh = rfh->GetMainFrame()->GetParentOrOuterDocument();
    DCHECK(rfh);
  }

  return rfh->GetPage();
}

}  // namespace subresource_filter
