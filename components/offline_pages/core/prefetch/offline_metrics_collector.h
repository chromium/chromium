// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_OFFLINE_METRICS_COLLECTOR_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_OFFLINE_METRICS_COLLECTOR_H_

namespace offline_pages {

// Observes various events and collects the data in order to classify
// a day as 'offline', 'online' etc. Keeps the accumulated counters of each day
// type until it is a good moment to report it (most often on a connected day).
// The actual reporting is done by UMA.
class OfflineMetricsCollector {
 public:
  virtual ~OfflineMetricsCollector() = default;

  // OfflineUsage histogram support.

  // Chrome started up or was (as on Android) brought from background to
  // foreground. A day when this happened is a day the browser was used.
  virtual void OnAppStartupOrResume() = 0;

  // Successful online navigation committed. A day when this happens is counted
  // as 'connected' day.
  virtual void OnSuccessfulNavigationOnline() = 0;

  // Successful navigation to an offline page happened. A day when it happens is
  // at least a 'offline_content' day
  virtual void OnSuccessfulNavigationOffline() = 0;

  // PrefetchUsage histogram support.

  // Reports that combination of flags resulting in Prefetch activation is set.
  // A day when it happens is reported as 'prefetch enabled' day.
  virtual void OnPrefetchEnabled() = 0;

  // A page was successfully prefetched. A day when it happens is reported as
  // 'successful prefetch' day.
  virtual void OnSuccessfulPagePrefetch() = 0;

  // A prefetched page was opened. A day when it happened is reported as
  // 'prefetch page opened' day.
  virtual void OnPrefetchedPageOpened() = 0;

  // Uses UMA to report the accumulated classification for the days past.
  virtual void ReportAccumulatedStats() = 0;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_OFFLINE_METRICS_COLLECTOR_H_
