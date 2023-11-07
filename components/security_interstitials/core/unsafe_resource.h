// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CORE_UNSAFE_RESOURCE_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CORE_UNSAFE_RESOURCE_H_

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/unguessable_token.h"
#include "components/safe_browsing/core/browser/db/hit_report.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "url/gurl.h"

namespace web {
class WebState;
}  // namespace web

namespace security_interstitials {

// Structure that passes parameters between the IO and UI thread when
// interacting with the safe browsing blocking page.
struct UnsafeResource {
  // Passed booleans indicating whether or not it is OK to proceed with
  // loading an URL and whether or not an interstitial was shown as a result of
  // the URL load, |showed_interstitial| should only be set to true if the
  // interstitial was shown as a direct result of the navigation to the URL.
  // (e.g. it should be set to true if the interstitial will be shown from a
  // navigation throttle triggered by this navigation, but to false if it will
  // be shown using LoadPostCommitErrorPage).
  using UrlCheckCallback =
      base::RepeatingCallback<void(bool /*proceed*/,
                                   bool /*showed_interstitial*/)>;

  // TODO(crbug.com/1073315): These are content/ specific ids that need to be
  // plumbed through this struct.
  // Equivalent to GlobalRenderFrameHostId.
  using RenderProcessId = int;
  using RenderFrameToken = std::optional<base::UnguessableToken>;
  // See RenderFrameHost::GetFrameTreeNodeId.
  using FrameTreeNodeId = int;
  // Copies of the sentinel values used in content/.
  // Equal to ChildProcessHost::kInvalidUniqueID.
  static constexpr RenderProcessId kNoRenderProcessId = -1;
  // Equal to RenderFrameHost::kNoFrameTreeNodeId.
  static constexpr FrameTreeNodeId kNoFrameTreeNodeId = -1;

  UnsafeResource();
  UnsafeResource(const UnsafeResource& other);
  ~UnsafeResource();

  // Returns true if this UnsafeResource is a main frame load that was blocked
  // while the navigation is still pending. Note that a main frame hit may not
  // be blocking, eg. client side detection happens after the load is
  // committed.
  bool IsMainPageLoadBlocked() const;

  // Checks if |callback| is not null and posts it to |callback_sequence|.
  void DispatchCallback(const base::Location& from_here,
                        bool proceed,
                        bool showed_interstitial) const;

  GURL url;
  GURL original_url;
  GURL navigation_url;
  GURL referrer_url;
  std::vector<GURL> redirect_urls;
  bool is_subresource;
  bool is_subframe;
  safe_browsing::SBThreatType threat_type;
  safe_browsing::ThreatMetadata threat_metadata;
  safe_browsing::RTLookupResponse rt_lookup_response;
  network::mojom::RequestDestination request_destination;
  UrlCheckCallback callback;  // This is called back on |callback_sequence|.
  scoped_refptr<base::SequencedTaskRunner> callback_sequence;
  // TODO(crbug.com/1073315): |weak_web_state| is only used on iOS, and
  // |render_process_id|, |render_frame_id|, and |frame_tree_node_id| are used
  // on all other platforms. This struct should be refactored to use only the
  // common functionality can be shared across platforms.
  // These content/ specific ids indicate what triggered safe browsing. In the
  // case of a frame navigating, we should have its FrameTreeNode id. In the
  // case of something triggered by a document (e.g. subresource loading), we
  // should have the RenderFrameHost's id.
  RenderProcessId render_process_id = kNoRenderProcessId;
  RenderFrameToken render_frame_token;
  FrameTreeNodeId frame_tree_node_id = kNoFrameTreeNodeId;

  base::WeakPtr<web::WebState> weak_web_state;

  safe_browsing::ThreatSource threat_source =
      safe_browsing::ThreatSource::UNKNOWN;
  // |token| field is only set if |threat_type| is
  // SB_THREAT_TYPE_*_PASSWORD_REUSE.
  std::string token;

  // If true, this UnsafeResource is created because of the Delayed Warnings
  // experiment.
  bool is_delayed_warning;
};

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CORE_UNSAFE_RESOURCE_H_
