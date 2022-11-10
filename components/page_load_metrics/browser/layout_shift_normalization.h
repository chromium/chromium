// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_LAYOUT_SHIFT_NORMALIZATION_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_LAYOUT_SHIFT_NORMALIZATION_H_

#include "base/time/time.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace page_load_metrics {

// LayoutShiftNormalization implements some experimental strategies for
// normalizing layout shift. Instead of adding all layout shift scores together
// like what we do in Cumulative Layout Shift(CLS), we aggregate layout shifts
// window by window. For more information, see go/layoutshiftnorm.
class LayoutShiftNormalization {
 public:
  LayoutShiftNormalization();

  LayoutShiftNormalization(const LayoutShiftNormalization&) = delete;
  LayoutShiftNormalization& operator=(const LayoutShiftNormalization&) = delete;

  ~LayoutShiftNormalization();
  const NormalizedCLSData& normalized_cls_data() const {
    return normalized_cls_data_;
  }

  void AddNewLayoutShifts(
      const std::vector<page_load_metrics::mojom::LayoutShiftPtr>& new_shifts,
      base::TimeTicks current_time,
      /*Whole page CLS*/ float cumulative_layout_shift_score);

  void ClearAllLayoutShifts();

 private:
  struct SessionWindow {
    base::TimeTicks start_time;
    base::TimeTicks last_time;
    float layout_shift_score = 0.0;
  };

  void UpdateWindowCLS(
      std::vector<std::pair<base::TimeTicks, double>>::const_iterator first,
      std::vector<std::pair<base::TimeTicks, double>>::const_iterator
          first_non_stale,
      std::vector<std::pair<base::TimeTicks, double>>::const_iterator last,
      float cumulative_layout_shift_score);

  void UpdateSessionWindow(
      SessionWindow* session_window,
      base::TimeDelta gap,
      base::TimeDelta max_duration,
      std::vector<std::pair<base::TimeTicks, double>>::const_iterator begin,
      std::vector<std::pair<base::TimeTicks, double>>::const_iterator end,
      float& max_score);

  // CLS normalization
  NormalizedCLSData normalized_cls_data_;

  // This vector is maintained in sorted order.
  std::vector<std::pair<base::TimeTicks, double>> recent_layout_shifts_;

  SessionWindow session_gap1000ms_max5000ms_;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_LAYOUT_SHIFT_NORMALIZATION_H_
