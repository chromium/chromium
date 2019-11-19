// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_RESOURCE_TRACKER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_RESOURCE_TRACKER_H_

#include <map>
#include <memory>
#include <vector>

#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "content/public/browser/global_request_id.h"

namespace page_load_metrics {

// Tracks individual resource loads on the page. Only tracks resources loaded
// by the network stack (including HTTP cache).
class ResourceTracker {
 public:
  // Maps a request id for a blink resource to the metadata for the resource
  // load.  GlobalRequestIDs are used because this map may aggregate across
  // renderers and blink request ids are unique per-renderer.
  using ResourceMap = std::map<content::GlobalRequestID,
                               page_load_metrics::mojom::ResourceDataUpdatePtr>;

  ResourceTracker();
  ~ResourceTracker();

  // Updates map of ongoing resource loads given a new update. |process_id| is
  // the id of renderer process where the update originated and is used to
  // create globally unique resource ids.
  void UpdateResourceDataUse(
      int process_id,
      const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
          resources);

  const ResourceMap& unfinished_resources() const {
    return unfinished_resources_;
  }

  // Previous ResourceDataUseUpdate objects are only stored for resources that
  // are still actively loaded. These methods should only be used for resources
  // that were received by observers in
  // PageLoadMetricsObserver::OnResourceDataUseObserved().
  bool HasPreviousUpdateForResource(content::GlobalRequestID request_id) const;
  const page_load_metrics::mojom::ResourceDataUpdatePtr&
  GetPreviousUpdateForResource(content::GlobalRequestID request_id) const;

 private:
  void ProcessResourceUpdate(
      int process_id,
      const page_load_metrics::mojom::ResourceDataUpdatePtr& resource);

  // Stores all resources that have started loading on the page but have
  // not completed loading.
  ResourceMap unfinished_resources_;

  // Maps a request_id for a blink resource to the previous ResourceDataUpdate
  // received for the resource. This only stores previous updates for
  // resources that are still actively receiving updates. A resource that was
  // unfinished then completes will have its previous update in this map until
  // the next call to UpdateResourceDataUse().
  ResourceMap previous_resource_updates_;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_RESOURCE_TRACKER_H_
