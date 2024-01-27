// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_AD_METRICS_PAGE_AD_DENSITY_TRACKER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_AD_METRICS_PAGE_AD_DENSITY_TRACKER_H_

#include <base/containers/flat_map.h>

#include <map>
#include <optional>
#include <set>

#include "base/memory/raw_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "components/page_load_metrics/browser/observers/ad_metrics/page_ad_density_tracker.h"
#include "components/page_load_metrics/browser/observers/ad_metrics/univariate_stats.h"
#include "ui/gfx/geometry/rect.h"

namespace page_load_metrics {

// Tracks the ad density of a page through the page's lifecycle.
// It has the following usage:
//    1. Set subframe, mainframe, and viewport rects using operations (AddRect,
//       RemoveRect, UpdateMainFrameRect, UpdateMainFrameViewportRect).
//    2. When the main frame rect or a subframe rect is updated, get current
//       page ad density using CalculatePageAdDensity.
//    3. When the main frame viewport rect or a subframe rect is updated, get
//       current viewport ad density using CalculateViewportAdDensity.
class PageAdDensityTracker {
 public:
  enum class RectType { kIFrame, kElement };

  struct RectId {
    RectId(RectType rect_type, int id);
    RectId(const RectId& other);

    RectType rect_type;

    // For iframe, the id comes from the frame tree node id. For other elements
    // (e.g. main frame ad rectangles), the id comes from the node id from the
    // renderer.
    int id;

    bool operator<(const RectId& rhs) const;
    bool operator==(const RectId& rhs) const;
    bool operator!=(const RectId& rhs) const;
  };

  struct AdDensityCalculationResult {
    std::optional<int> ad_density_by_height;
    std::optional<int> ad_density_by_area;
  };

  explicit PageAdDensityTracker(base::TickClock* clock = nullptr);
  ~PageAdDensityTracker();

  PageAdDensityTracker(const PageAdDensityTracker&) = delete;
  PageAdDensityTracker& operator=(const PageAdDensityTracker&) = delete;

  // Operations to track sub frame rects in the page density calcluation. If
  // `recalculate_density` is true, the max page ad density and the viewport ad
  // density will be recalculated in the end.
  void AddRect(RectId rect_id, const gfx::Rect& rect, bool recalculate_density);

  // Removes a rect from the tracker if it is currently being tracked.
  // Otherwise RemoveRect is a no op. If `recalculate_viewport_density` is true,
  // the viewport ad density will be recalculated in the end.
  void RemoveRect(RectId rect_id, bool recalculate_viewport_density);

  // Operations to track the main frame dimensions. The main frame rect has to
  // be set to calculate the page ad density.
  void UpdateMainFrameRect(const gfx::Rect& rect);

  // Operations to track the main frame viewport position and dimensions. This
  // rect has to be set to calculate the viewport ad density.
  void UpdateMainFrameViewportRect(const gfx::Rect& rect);

  // Operations to track the main frame ad rectangles' position and dimensions.
  void UpdateMainFrameImageAdRects(
      const base::flat_map<int, gfx::Rect>& main_frame_image_ad_rects);

  // Returns the density by height, as a value from 0-100. If the density
  // calculation fails (i.e. no main frame size), this returns -1. Percentage
  // density by height is calculated as the the combined height of ads divided
  // by the page's height.
  int MaxPageAdDensityByHeight() const;

  // Returns the density by area, as a value from 0-100. If the density
  // calculation fails (i.e. no main frame size), this returns -1.
  int MaxPageAdDensityByArea() const;

  // Returns the distribution moments of the viewport ad density by area.
  // Returns default value (i.e. 0s) if the density calculation didn't happen
  // (i.e. no main frame viewport) or if the elapsed time is 0.
  UnivariateStats::DistributionMoments GetAdDensityByAreaStats() const;

  // Returns the last calculated viewport ad density by area, as a value from
  // 0-100. If the density calculation didn't happen (i.e. no main frame
  // viewport), this returns 0.
  int ViewportAdDensityByArea() const;

  // Called at the end of the page load to finalize metrics measurement.
  void Finalize();

  // Only for test purpose.
  friend class PageAdDensityTrackerTestPeer;

 private:
  // An event to process corresponding to the top or bottom of each rect.
  struct RectEvent {
    RectEvent(RectId id, bool is_bottom, const gfx::Rect& rect);
    RectEvent(const RectEvent& other);

    // A unique identifier set when adding and removing rect events
    // corresponding to a single rect.
    RectId rect_id;
    bool is_bottom;
    gfx::Rect rect;

    // RectEvents are sorted by descending y value of the segment associated
    // with the event.
    bool operator<(const RectEvent& rhs) const;
  };

  // Iterators into the set of rect events for efficient removal of
  // rect events by rect_id. Maintained by |rect_events_iterators_|.
  struct RectEventSetIterators {
    RectEventSetIterators(std::set<RectEvent>::iterator top,
                          std::set<RectEvent>::iterator bottom);
    RectEventSetIterators(const RectEventSetIterators& other);

    std::set<RectEvent>::const_iterator top_it;
    std::set<RectEvent>::const_iterator bottom_it;
  };

  // Accumulate `last_viewport_ad_density_by_area_` and its weight (i.e. the
  // elapsed time since `last_viewport_density_accumulate_time_`) into
  // `viewport_ad_density_by_area_stats_`. This can be invoked either when a
  // new density is calculated, or during `Finalize()`.
  void AccumulateOutstandingViewportAdDensity();

  void CalculatePageAdDensity();
  void CalculateViewportAdDensity();

  // Calculates the combined area and height of the set of rects bounded by
  // `bounding_rect`, and further derive the ad density by area and height.
  AdDensityCalculationResult CalculateDensityWithin(
      const gfx::Rect& bounding_rect);

  // Maintain a sorted set of rect events for use in calculating ad area.
  std::set<RectEvent> rect_events_;

  // Map from rect_id to iterators of rect events in rect_events_. This allows
  // efficient removal according to rect_id.
  std::map<RectId, RectEventSetIterators> rect_events_iterators_;

  // Percentage of page ad density as a value from 0-100. These only have
  // a value of -1 when ad density has not yet been calculated successfully.
  int max_page_ad_density_by_area_ = -1;
  int max_page_ad_density_by_height_ = -1;

  // The last main frame size (a rectangle at position (0,0)).
  gfx::Rect last_main_frame_rect_;

  // The last main frame viewport rectangle in `last_main_frame_rect_`'s
  // coordinate system.
  gfx::Rect last_main_frame_viewport_rect_;

  // The last time when `last_viewport_ad_density_by_area_` is accumulated into
  // `viewport_ad_density_by_area_stats_`. Set to the current time at the start
  // of the page.
  base::TimeTicks last_viewport_density_accumulate_time_;

  // The last calculated ad density within the main frame viewport.
  int last_viewport_ad_density_by_area_ = 0;

  // Keep observing `last_viewport_ad_density_by_area_` before each time it gets
  // an update, to derive the distribution statistics in the end.
  UnivariateStats viewport_ad_density_by_area_stats_;

  bool finalize_called_ = false;

  // The tick clock used to get the current time. Can be replaced by tests.
  raw_ptr<const base::TickClock> clock_;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_AD_METRICS_PAGE_AD_DENSITY_TRACKER_H_
