// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_CLIENT_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_CLIENT_H_

#include "base/memory/scoped_refptr.h"
#include "components/subresource_filter/content/browser/verified_ruleset_dealer.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace safe_browsing {
class SafeBrowsingDatabaseManager;
}

namespace subresource_filter {

class SubresourceFilterClient {
 public:
  virtual ~SubresourceFilterClient() = default;

  // Informs the embedder to show some UI indicating that resources are being
  // blocked.
  virtual void ShowNotification() = 0;

  // Called when the activation decision is otherwise completely computed by the
  // subresource filter. At this point, the embedder still has a chance to
  // alter the effective activation. Returns the effective activation for this
  // navigation.
  //
  // Note: |decision| is guaranteed to be non-nullptr, and can be modified by
  // the embedder if any decision changes.
  //
  // Precondition: The navigation must be a main frame navigation.
  virtual mojom::ActivationLevel OnPageActivationComputed(
      content::NavigationHandle* navigation_handle,
      mojom::ActivationLevel initial_activation_level,
      subresource_filter::ActivationDecision* decision) = 0;

  // Called on the subresource filter client when an ads violation is detected.
  virtual void OnAdsViolationTriggered(
      content::RenderFrameHost* rfh,
      mojom::AdsViolation triggered_violation) = 0;

  // Returns the SafeBrowsingDatabaseManager instance associated with this
  // client, or null if there is no such instance.
  virtual const scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
  GetSafeBrowsingDatabaseManager() = 0;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_CLIENT_H_
