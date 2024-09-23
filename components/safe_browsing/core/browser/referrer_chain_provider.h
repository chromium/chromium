// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_REFERRER_CHAIN_PROVIDER_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_REFERRER_CHAIN_PROVIDER_H_

#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/sessions/core/session_id.h"
#include "url/gurl.h"

namespace content {
class RenderFrameHost;
struct GlobalRenderFrameHostId;
}

namespace safe_browsing {
using ReferrerChain =
    google::protobuf::RepeatedPtrField<safe_browsing::ReferrerChainEntry>;

class ReferrerChainProvider {
 public:
  // For UMA histogram counting. Do NOT change order.
  enum AttributionResult {
    SUCCESS = 1,                   // Identified referrer chain is not empty.
    SUCCESS_LANDING_PAGE = 2,      // Successfully identified landing page.
    SUCCESS_LANDING_REFERRER = 3,  // Successfully identified landing referrer.
    INVALID_URL = 4,
    NAVIGATION_EVENT_NOT_FOUND = 5,
    SUCCESS_REFERRER = 6,  // Successfully identified extra referrers beyond the
                           // landing referrer.

    // Always at the end.
    ATTRIBUTION_FAILURE_TYPE_MAX
  };

  virtual AttributionResult IdentifyReferrerChainByRenderFrameHost(
      content::RenderFrameHost* render_frame_host,
      int user_gesture_count_limit,
      ReferrerChain* out_referrer_chain) = 0;

  virtual AttributionResult IdentifyReferrerChainByEventURL(
      const GURL& event_url,
      SessionID event_tab_id,
      const content::GlobalRenderFrameHostId& event_outermost_main_frame_id,
      int user_gesture_count_limit,
      ReferrerChain* out_referrer_chain) = 0;

  virtual AttributionResult IdentifyReferrerChainByEventURL(
      const GURL& event_url,
      SessionID event_tab_id,
      int user_gesture_count_limit,
      ReferrerChain* out_referrer_chain) = 0;

  virtual AttributionResult IdentifyReferrerChainByPendingEventURL(
      const GURL& event_url,
      int user_gesture_count_limit,
      ReferrerChain* out_referrer_chain) = 0;
};
}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_REFERRER_CHAIN_PROVIDER_H_
