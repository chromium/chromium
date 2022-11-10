// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/layout_shift_normalization.h"

namespace page_load_metrics {
constexpr auto NEW_SHIFT_BUFFER_WINDOW_DURATION = base::Seconds(5);
constexpr auto MAX_SHIFT_BUFFER_SIZE = 300;

LayoutShiftNormalization::LayoutShiftNormalization() = default;
LayoutShiftNormalization::~LayoutShiftNormalization() = default;

void LayoutShiftNormalization::AddNewLayoutShifts(
    const std::vector<page_load_metrics::mojom::LayoutShiftPtr>& new_shifts,
    base::TimeTicks current_time,
    float cumulative_layout_shift_score) {
  if (new_shifts.empty() || normalized_cls_data_.data_tainted)
    return;

  if ((current_time - new_shifts[0]->layout_shift_time >
       NEW_SHIFT_BUFFER_WINDOW_DURATION) ||
      (recent_layout_shifts_.size() + new_shifts.size() >
       MAX_SHIFT_BUFFER_SIZE)) {
    // We can't keep all layout shifts all the time especially for long lived
    // pages. So we allow a layout shift to be delivered to the browser
    // process within 5 seconds and set a limit for the number of shifts we can
    // store in the browser process. If the delivery latency of a layout shift
    // is bigger than 5s, we can't process this layout shift and get the correct
    // normalization results, which means we should not report the results in
    // UKM.
    normalized_cls_data_.data_tainted = true;
    return;
  }

  // Append all shift data.
  for (const auto& layout_shift : new_shifts) {
    recent_layout_shifts_.emplace_back(layout_shift->layout_shift_time,
                                       layout_shift->layout_shift_score);
  }

  // At this point, the vector contains two sorted subvectors, and we can do an
  // in-place merge to create a sorted vector.
  std::inplace_merge(recent_layout_shifts_.begin(),
                     recent_layout_shifts_.end() - new_shifts.size(),
                     recent_layout_shifts_.end());

  // Now that we have a single sorted list of shifts, we can find the partition
  // point for old shifts which we are ready to delete
  auto first_non_stale = std::upper_bound(
      recent_layout_shifts_.begin(), recent_layout_shifts_.end(),
      current_time - NEW_SHIFT_BUFFER_WINDOW_DURATION,
      [](auto time, auto const& shift) { return time < shift.first; });

  // Update sliding and session window CLS.
  UpdateWindowCLS(recent_layout_shifts_.begin(), first_non_stale,
                  recent_layout_shifts_.end(), cumulative_layout_shift_score);

  // Finally, remove the stale shifts at this point.
  recent_layout_shifts_.erase(recent_layout_shifts_.begin(), first_non_stale);
}

void LayoutShiftNormalization::ClearAllLayoutShifts() {
  normalized_cls_data_ = NormalizedCLSData();
  recent_layout_shifts_.clear();
  session_gap1000ms_max5000ms_ = SessionWindow();
}

void LayoutShiftNormalization::UpdateSessionWindow(
    SessionWindow* session_window,
    base::TimeDelta gap,
    base::TimeDelta max_duration,
    std::vector<std::pair<base::TimeTicks, double>>::const_iterator begin,
    std::vector<std::pair<base::TimeTicks, double>>::const_iterator end,
    float& max_score) {
  for (auto it = begin; it != end; ++it) {
    if ((it->first - session_window->last_time > gap) ||
        (it->first - session_window->start_time > max_duration)) {
      session_window->start_time = it->first;
      session_window->layout_shift_score = 0;
    }
    session_window->last_time = it->first;
    session_window->layout_shift_score += it->second;
    max_score = std::max(max_score, session_window->layout_shift_score);
  }
}

// This function will update layout shift normalization results for sliding
// windows and session windows twice. The first update is processing stale
// layout shifts (current_time - layout_shift_time >= 5s) in
// recent_layout_shifts_. The second update is continuing the first update by
// processing remaining layout shifts in recent_layout_shifts_. Processing
// layout shifts in order is very important to our algorithm or we may
// miscalculate the number of windows or layout shift score for a window.
// We assume the browser process can receive any layout shift within 5 seconds,
// so there might be some non-stale layout shifts on the way and a newer layout
// shift can be received earlier than an old one. This is one reason why we
// update layout shift normalization twice. Another reason is we can't updating
// normalization on demand when we record UKM because of constant class objects.
// The first update is for updating the normalization for all stale layout shift
// we received. The second update is for all layout shifts we received so far in
// case we need to record UKM right away.
void LayoutShiftNormalization::UpdateWindowCLS(
    std::vector<std::pair<base::TimeTicks, double>>::const_iterator first,
    std::vector<std::pair<base::TimeTicks, double>>::const_iterator
        first_non_stale,
    std::vector<std::pair<base::TimeTicks, double>>::const_iterator last,
    float cumulative_layout_shift_score) {
  // Update Session Windows.
  UpdateSessionWindow(
      &session_gap1000ms_max5000ms_, base::Milliseconds(1000),
      base::Milliseconds(5000), first, first_non_stale,
      normalized_cls_data_.session_windows_gap1000ms_max5000ms_max_cls);
  auto tmp_session_gap1000ms_max5000ms = session_gap1000ms_max5000ms_;

  UpdateSessionWindow(
      &tmp_session_gap1000ms_max5000ms, base::Milliseconds(1000),
      base::Milliseconds(5000), first_non_stale, last,
      normalized_cls_data_.session_windows_gap1000ms_max5000ms_max_cls);
}
}  // namespace page_load_metrics
