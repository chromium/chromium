// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PAGE_LOAD_METRICS_PAGE_TIMING_SENDER_H_
#define CHROME_RENDERER_PAGE_LOAD_METRICS_PAGE_TIMING_SENDER_H_

#include "chrome/common/page_load_metrics/page_load_metrics.mojom.h"

namespace page_load_metrics {

// PageTimingSender is an interface that is responsible for sending page load
// timing through IPC.
class PageTimingSender {
 public:
  virtual ~PageTimingSender() {}
  virtual void SendTiming(const mojom::PageLoadTimingPtr& timing,
                          const mojom::PageLoadMetadataPtr& metadata,
                          mojom::PageLoadFeaturesPtr new_features,
                          std::vector<mojom::ResourceDataUpdatePtr> resources,
                          const mojom::PageRenderData& render_data) = 0;
};

}  // namespace page_load_metrics

#endif  // CHROME_RENDERER_PAGE_LOAD_METRICS_PAGE_TIMING_SENDER_H_
