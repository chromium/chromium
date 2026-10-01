// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_NAVIGATION_HANDLE_USER_DATA_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_NAVIGATION_HANDLE_USER_DATA_H_

#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_user_data.h"

namespace page_load_metrics {

// Records that the navigation was served by DSEv1 search prefetch.
//
// History: This used to carry the initiator of the navigation too, as a pair
// of `(InitiatorLocation, std::string)`, which is where the generic name of
// this class comes from. That role was taken over by
// `page_load_metrics::NavigationInitiator` and `NavigationInitiatorHolder`,
// and the DSEv1 search prefetch flag is all that is left.
//
// Timing of availability:
// This user data is attached to a `content::NavigationHandle` while the
// navigation is in flight, so it is NOT guaranteed to be present during
// `PageLoadMetricsObserver::OnStart()`.
// `PageLoadMetricsObserver::OnCommit()` (or `DidActivatePrerenderedPage()` for
// prerender activation) is a reliable time to retrieve this data. Note that
// once the navigation has finished committing, the `NavigationHandle` is
// destroyed, making the user data no longer accessible.
//
// TODO(https://crbug.com/517725655): Rename this to something that tells what
// it carries, now that it no longer carries the navigation initiator.
class NavigationHandleUserData
    : public content::NavigationHandleUserData<NavigationHandleUserData> {
 public:
  ~NavigationHandleUserData() override;

  bool is_served_by_legacy_search_prefetch() const {
    return is_served_by_legacy_search_prefetch_;
  }
  void set_is_served_by_legacy_search_prefetch(bool is_served) {
    is_served_by_legacy_search_prefetch_ = is_served;
  }

 private:
  explicit NavigationHandleUserData(content::NavigationHandle& navigation);

  // Indicates whether this navigation was served by a legacy search prefetch
  // mechanism (i.e., DSEv1 search prefetch). Legacy search prefetch refers to
  // embedder-managed search prefetch mechanisms that operate outside and
  // predate the unified `content::PrefetchService` preloading pipeline.
  //
  // Because such prefetch requests are handled directly by the embedder rather
  // than the content layer, they do not automatically integrate with
  // `content::PreloadServingMetrics`. This variable allows the embedder to
  // signal that the navigation was served by the legacy search prefetch so that
  // `PreloadServingMetricsPageLoadMetricsObserver` can accurately record
  // preload serving metrics.
  bool is_served_by_legacy_search_prefetch_ = false;

  friend content::NavigationHandleUserData<NavigationHandleUserData>;
  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_NAVIGATION_HANDLE_USER_DATA_H_
