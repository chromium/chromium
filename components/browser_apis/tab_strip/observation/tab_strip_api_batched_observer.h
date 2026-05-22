// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_STRIP_OBSERVATION_TAB_STRIP_API_BATCHED_OBSERVER_H_
#define COMPONENTS_BROWSER_APIS_TAB_STRIP_OBSERVATION_TAB_STRIP_API_BATCHED_OBSERVER_H_

#include "base/observer_list_types.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"

namespace tabs_api::observation {

// A direct observer for the tab strip service. Clients should use
// tabs_apis::observation::Observation class for method specific dispatch.
class TabStripApiBatchedObserver : public base::CheckedObserver {
 public:
  ~TabStripApiBatchedObserver() override = default;

  virtual void OnTabEvents(const std::vector<mojom::TabsEventPtr>& events) = 0;
};

}  // namespace tabs_api::observation

#endif  // COMPONENTS_BROWSER_APIS_TAB_STRIP_OBSERVATION_TAB_STRIP_API_BATCHED_OBSERVER_H_
