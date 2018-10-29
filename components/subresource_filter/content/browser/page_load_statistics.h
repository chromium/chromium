// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_PAGE_LOAD_STATISTICS_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_PAGE_LOAD_STATISTICS_H_

#include "base/macros.h"
#include "components/subresource_filter/mojom/subresource_filter.mojom.h"

namespace subresource_filter {

// This class is notified of metrics recorded for individual (sub-)documents of
// a page, aggregates them, and logs the aggregated metrics to UMA histograms
// when the page load is complete (at the load event).
class PageLoadStatistics {
 public:
  PageLoadStatistics(const mojom::ActivationState& state);
  ~PageLoadStatistics();

  void OnDocumentLoadStatistics(
      const mojom::DocumentLoadStatistics& statistics);
  void OnDidFinishLoad();

 private:
  mojom::ActivationState activation_state_;

  // Statistics about subresource loads, aggregated across all frames of the
  // current page.
  mojom::DocumentLoadStatistics aggregated_document_statistics_;

  DISALLOW_COPY_AND_ASSIGN(PageLoadStatistics);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_PAGE_LOAD_STATISTICS_H_
