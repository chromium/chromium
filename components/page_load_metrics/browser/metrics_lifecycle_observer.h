// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_METRICS_LIFECYCLE_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_METRICS_LIFECYCLE_OBSERVER_H_

#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"

#include "base/memory/raw_ptr.h"
#include "base/observer_list_types.h"
#include "content/public/browser/web_contents.h"

namespace page_load_metrics {

class PageLoadTracker;

// |MetricsLifecycleObserver| allows clients to observe lifecycle events for a
// given |MetricsWebContentsObserver|. It is only used in testing but will work
// as intended if used in production.
class MetricsLifecycleObserver : public base::CheckedObserver {
 public:
  explicit MetricsLifecycleObserver(content::WebContents* web_contents);

  MetricsLifecycleObserver(const MetricsLifecycleObserver&) = delete;
  MetricsLifecycleObserver& operator=(const MetricsLifecycleObserver&) = delete;
  MetricsLifecycleObserver(MetricsLifecycleObserver&&) = delete;
  MetricsLifecycleObserver& operator=(MetricsLifecycleObserver&&) = delete;

  ~MetricsLifecycleObserver() override;

  void OnGoingAway();

  // Some |PageLoadTiming| messages will race with the navigation commit.
  // |OnTrackerCreated()| allows client code to manipulate the
  // |PageLoadTracker| very early (eg, to add observers).
  virtual void OnTrackerCreated(PageLoadTracker* tracker) {}

  // In cases where |LoadTimingInfo| is not needed, waiting until commit is
  // fine.
  virtual void OnCommit(PageLoadTracker* tracker) {}

  // This is called both for prerender activation and restoration from
  // the back/forward cache.
  virtual void OnActivate(PageLoadTracker* tracker) {}

  // Returns the observer delegate for the committed load associated with
  // the |MetricsWebContentsObserver|, or null if the observer has gone away
  // (via MetricsWebContentsObserver::WebContentsDestroyed).
  const PageLoadMetricsObserverDelegate* GetDelegateForCommittedLoad();

 private:
  raw_ptr<MetricsWebContentsObserver> observer_;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_METRICS_LIFECYCLE_OBSERVER_H_
