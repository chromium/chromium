// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_STATS_TRACKER_H_
#define CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_STATS_TRACKER_H_

#include <set>

#include "base/no_destructor.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

class ThumbnailImage;

// Records memory metrics across all thumbnails in a browser process.
class ThumbnailStatsTracker {
 private:
  friend class ThumbnailImage;
  friend class base::NoDestructor<ThumbnailStatsTracker>;

  friend class ThumbnailStatsTrackerTest;

  static constexpr base::TimeDelta kReportingInterval = base::Minutes(5);

  // Gets the global instance for this process.
  static ThumbnailStatsTracker& GetInstance();

  // This must only be called if all registered thumbnails have been
  // removed.
  static void ResetInstanceForTesting();

  ThumbnailStatsTracker();

  // Exists only for ResetInstanceForTesting().
  ~ThumbnailStatsTracker();

  // Called from our friend, ThumbnailImage.
  void AddThumbnail(ThumbnailImage* thumbnail);
  void RemoveThumbnail(ThumbnailImage* thumbnail);

  // Called by |heartbeat_timer_| to record metrics at a regular
  // interval.
  void RecordMetrics();

  base::RepeatingTimer heartbeat_timer_;

  std::set<ThumbnailImage*> thumbnails_;
};

#endif  // CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_STATS_TRACKER_H_
