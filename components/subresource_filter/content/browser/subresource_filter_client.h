// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_CLIENT_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_CLIENT_H_

#include "base/memory/scoped_refptr.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/render_frame_host.h"

namespace safe_browsing {
class SafeBrowsingDatabaseManager;
}

namespace subresource_filter {

class ProfileInteractionManager;

class SubresourceFilterClient {
 public:
  virtual ~SubresourceFilterClient() = default;

  // Informs the embedder to show some UI indicating that resources are being
  // blocked. This method will be called at most once per main-frame navigation.
  virtual void ShowNotification() = 0;

  // Called on the subresource filter client when an ads violation is detected.
  virtual void OnAdsViolationTriggered(
      content::RenderFrameHost* rfh,
      mojom::AdsViolation triggered_violation) = 0;

  // Returns the SafeBrowsingDatabaseManager instance associated with this
  // client, or null if there is no such instance.
  virtual const scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
  GetSafeBrowsingDatabaseManager() = 0;

  // Returns the ProfileInteractionManager instance associated with this
  // client, or null if there is no such instance.
  // TODO(crbug.com/1116095): Have ContentSubresourceFilterThrottleManager
  // create and own this object internally once ChromeSubresourceFilterClient no
  // longer calls into it, replacing this method with a getter for
  // SubresourceFilterProfileContext.
  virtual ProfileInteractionManager* GetProfileInteractionManager() = 0;

  // Invoked when the user has requested a reload of a page with blocked ads
  // (e.g., via an infobar).
  virtual void OnReloadRequested() = 0;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_CLIENT_H_
