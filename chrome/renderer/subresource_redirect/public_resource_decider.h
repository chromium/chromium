// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SUBRESOURCE_REDIRECT_PUBLIC_RESOURCE_DECIDER_H_
#define CHROME_RENDERER_SUBRESOURCE_REDIRECT_PUBLIC_RESOURCE_DECIDER_H_

#include "base/bind.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/subresource_redirect/common/subresource_redirect_result.h"
#include "url/gurl.h"

namespace subresource_redirect {

// Different states the public resource decider redirection can be in.
enum class PublicResourceDeciderRedirectState {
  kNone = 0,

  // The redirect decision is pending from the underlying decider.
  kRedirectDecisionPending = 1,

  // Redirect was disallowed by the underlying decider e.g., robots rules
  // decider.
  kRedirectNotAllowedByDecider = 2,

  // The subresource request was redirected to attempt to compress it.
  kRedirectAttempted = 3,

  // Failed due to http response codes, net errors, and the subresource was
  // fetched from original origin.
  kRedirectFailed = 4,

  // The subresource request was allowed to be redirect compressed.
  kRedirectAllowed = 5,

  kMax = kRedirectAllowed,
};

// Interface for the decider agent classes that decide whether a resource is
// considered public and eligible for redirection for compression. Also allows
// coverage metrics to be recorded for the resource load.
class PublicResourceDecider {
 public:
  using ShouldRedirectDecisionCallback =
      base::OnceCallback<void(SubresourceRedirectResult)>;

  // Determine whether the subresource url should be redirected. When the
  // determination can be made immediately, the decision should be returned.
  // Otherwise base::nullopt should be returned and the callback should be
  // invoked with the decision asynchronously.
  virtual base::Optional<SubresourceRedirectResult> ShouldRedirectSubresource(
      const GURL& url,
      ShouldRedirectDecisionCallback callback) = 0;

  // Notifies the decider that the subresource load finished.
  virtual void RecordMetricsOnLoadFinished(
      const GURL& url,
      int64_t content_length,
      SubresourceRedirectResult redirect_result) = 0;

  // Notifies that compressed resource fetch had failed. |retry_after| indicates
  // the duration returned by the compression server, until which subsequent
  // fetches to compression server should be blocked.
  virtual void NotifyCompressedResourceFetchFailed(
      base::TimeDelta retry_after) = 0;

  // Returns the start time of the current navigation.
  virtual base::TimeTicks GetNavigationStartTime() const = 0;
};

}  // namespace subresource_redirect

#endif  // CHROME_RENDERER_SUBRESOURCE_REDIRECT_PUBLIC_RESOURCE_DECIDER_H_
