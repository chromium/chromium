// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_BROWSER_PAGE_LOAD_STATISTICS_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_BROWSER_PAGE_LOAD_STATISTICS_H_

#include <optional>
#include <string_view>

#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
}  // namespace content
namespace subresource_filter {

// This class is notified of metrics recorded for individual (sub-)documents of
// a page, aggregates them, and logs the aggregated metrics to UMA histograms
// when the page load is complete (at the load event).
class PageLoadStatistics {
 public:
  // TODO(crbug.com/341798380): The `navigation_handle`, `frame_host` and
  // `DetailForCrashKeys` are temporary and should be removed once the
  // associated CHECK failure is fixed.
  PageLoadStatistics(const mojom::ActivationState& state,
                     std::string_view uma_filter_tag,
                     content::NavigationHandle* navigation_handle,
                     content::RenderFrameHost* frame_host);

  // Constructor that does not populate `details_for_crash_keys_`.
  PageLoadStatistics(const mojom::ActivationState& state,
                     std::string_view uma_filter_tag);

  PageLoadStatistics(const PageLoadStatistics&) = delete;
  PageLoadStatistics& operator=(const PageLoadStatistics&) = delete;

  ~PageLoadStatistics();

  void OnDocumentLoadStatistics(
      const mojom::DocumentLoadStatistics& statistics);
  void OnDidFinishLoad();

 private:
  struct DetailsForCrashKeys {
    GURL navigation_url;
    net::Error navigation_error_code;
    bool has_navigation_committed;
    GURL frame_last_commited_url;
  };

  mojom::ActivationState activation_state_;
  std::string_view uma_filter_tag_;

  // Statistics about subresource loads, aggregated across all frames of the
  // current page.
  mojom::DocumentLoadStatistics aggregated_document_statistics_;

  std::optional<DetailsForCrashKeys> details_for_crash_keys_;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_BROWSER_PAGE_LOAD_STATISTICS_H_
