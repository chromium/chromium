// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_GOOGLE_BROWSER_PRERENDER_PREWARM_NAVIGATION_DATA_H_
#define COMPONENTS_PAGE_LOAD_METRICS_GOOGLE_BROWSER_PRERENDER_PREWARM_NAVIGATION_DATA_H_

#include "content/public/browser/navigation_handle_user_data.h"

namespace page_load_metrics {

// Communicates information about DSE prewarm navigations to
// PageLoadMetricsObservers. The data is owned by the NavigationHandle.
class PrerenderPrewarmNavigationData
    : public content::NavigationHandleUserData<PrerenderPrewarmNavigationData> {
 public:
  // The status of the DSE prewarm navigation. These values are persisted to
  // logs. Entries should not be renumbered and numeric values should never be
  // reused.
  // LINT.IfChange(PrerenderPrewarmNavigationStatus)
  enum class PrerenderPrewarmNavigationStatus {
    kNotPrewarmedNotReused = 0,
    kNotPrewarmedReused = 1,
    kPrewarmedNotReused = 2,
    kPrewarmedReused = 3,
    kMaxValue = kPrewarmedReused,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/page/enums.xml:PrerenderPrewarmNavigationStatus)

  ~PrerenderPrewarmNavigationData() override = default;

  PrerenderPrewarmNavigationStatus GetNavigationStatus() const;

  void SetPrerenderHostReused(bool prerender_host_reused) {
    prerender_host_reused_ = prerender_host_reused;
  }

 private:
  // Creates a PrerenderPrewarmNavigationData. `is_dse_prewarm` is true when
  // the navigation is the DSE prewarm itself. `prerender_host_reused` is true
  // when the prerender host was reused for this navigation.
  PrerenderPrewarmNavigationData(content::NavigationHandle& navigation_handle,
                                 bool prewarm_committed,
                                 bool prerender_host_reused);

  // True when webcontent had dse prewarm.
  const bool prewarm_committed_;

  // True if the prerender host was reused for this navigation.
  bool prerender_host_reused_;

  friend content::NavigationHandleUserData<PrerenderPrewarmNavigationData>;
  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_GOOGLE_BROWSER_PRERENDER_PREWARM_NAVIGATION_DATA_H_
