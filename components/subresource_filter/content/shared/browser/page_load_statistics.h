// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_BROWSER_PAGE_LOAD_STATISTICS_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_BROWSER_PAGE_LOAD_STATISTICS_H_

#include <string_view>

#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"

namespace subresource_filter {

// This class is notified of metrics recorded for individual (sub-)documents of
// a page, aggregates them, and logs the aggregated metrics to UMA histograms
// when the page load is complete (at the load event).
class PageLoadStatistics {
 public:
  PageLoadStatistics(const mojom::ActivationState& state,
                     std::string_view uma_filter_tag);

  PageLoadStatistics(const PageLoadStatistics&) = delete;
  PageLoadStatistics& operator=(const PageLoadStatistics&) = delete;

  ~PageLoadStatistics();

  void OnDocumentLoadStatistics(
      const mojom::DocumentLoadStatistics& statistics);
  void OnDidFinishLoad();

 private:
  mojom::ActivationState activation_state_;
  std::string_view uma_filter_tag_;

  // Statistics about subresource loads, aggregated across all frames of the
  // current page.
  mojom::DocumentLoadStatistics aggregated_document_statistics_;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_BROWSER_PAGE_LOAD_STATISTICS_H_
