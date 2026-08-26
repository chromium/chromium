// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/horizontal_tab_strip_metrics.h"

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "ui/events/event.h"
#include "ui/views/controls/scroll_view.h"

namespace tabs {

namespace {

// The minimum time needed between scroll events for us to log them as
// two separate scrolls, for metric recording purposes.
constexpr base::TimeDelta kScrollDebounceInterval = base::Milliseconds(500);

// The interval for periodically logging whether the unpinned tabs
// are scrollable.
constexpr base::TimeDelta kScrollableMetricsInterval = base::Minutes(5);

}  // namespace

void RecordHorizontalTabStripScrollSource(
    HorizontalTabStripScrollSource source) {
  base::UmaHistogramEnumeration("TabStrip.Horizontal.ScrollSource", source);
}

void RecordHorizontalTabStripIsScrollable(bool is_scrollable) {
  base::UmaHistogramBoolean("TabStrip.Horizontal.IsScrollable", is_scrollable);
}

UnpinnedTabScrollTypeRecorder::UnpinnedTabScrollTypeRecorder(
    views::ScrollView* scroll_view)
    : scroll_view_(scroll_view) {
  CHECK(scroll_view_);
  scroll_view_->AddPreTargetHandler(this);
}

UnpinnedTabScrollTypeRecorder::~UnpinnedTabScrollTypeRecorder() {
  if (scroll_view_) {
    scroll_view_->RemovePreTargetHandler(this);
  }
}

void UnpinnedTabScrollTypeRecorder::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::EventType::kMousewheel) {
    const auto* wheel = event->AsMouseWheelEvent();
    if (wheel->x_offset() != 0 || wheel->y_offset() != 0) {
      MaybeLogScrollHistogram();
    }
  }
}

void UnpinnedTabScrollTypeRecorder::OnScrollEvent(ui::ScrollEvent* event) {
  if (event->x_offset() != 0.0f || event->y_offset() != 0.0f) {
    MaybeLogScrollHistogram();
  }
}

void UnpinnedTabScrollTypeRecorder::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::EventType::kGestureScrollBegin ||
      event->type() == ui::EventType::kGestureScrollUpdate) {
    MaybeLogScrollHistogram();
  }
}

void UnpinnedTabScrollTypeRecorder::MaybeLogScrollHistogram() {
  if (!scroll_view_->IsHorizontalContentOverflowing()) {
    return;
  }
  const base::TimeTicks now = base::TimeTicks::Now();
  if (last_scroll_time_.is_null() ||
      (now - last_scroll_time_) > kScrollDebounceInterval) {
    RecordHorizontalTabStripScrollSource(
        HorizontalTabStripScrollSource::kTouchpadOrMouseWheel);
  }
  last_scroll_time_ = now;
}

UnpinnedTabScrollableStateRecorder::UnpinnedTabScrollableStateRecorder(
    views::ScrollView* scroll_view)
    : scroll_view_(scroll_view) {
  CHECK(scroll_view_);
  timer_.Start(FROM_HERE, kScrollableMetricsInterval,
               base::BindRepeating(
                   &UnpinnedTabScrollableStateRecorder::RecordScrollableMetric,
                   base::Unretained(this)));
}

UnpinnedTabScrollableStateRecorder::~UnpinnedTabScrollableStateRecorder() =
    default;

void UnpinnedTabScrollableStateRecorder::SetTaskRunnerForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  timer_.Stop();
  timer_.SetTaskRunner(std::move(task_runner));
  timer_.Start(FROM_HERE, kScrollableMetricsInterval,
               base::BindRepeating(
                   &UnpinnedTabScrollableStateRecorder::RecordScrollableMetric,
                   base::Unretained(this)));
}

void UnpinnedTabScrollableStateRecorder::RecordScrollableMetric() {
  RecordHorizontalTabStripIsScrollable(
      scroll_view_->IsHorizontalContentOverflowing());
}

}  // namespace tabs
