// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_HORIZONTAL_TAB_STRIP_METRICS_H_
#define CHROME_BROWSER_UI_TABS_HORIZONTAL_TAB_STRIP_METRICS_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/events/event_handler.h"

namespace base {
class SequencedTaskRunner;
}

namespace views {
class ScrollView;
}

namespace tabs {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(HorizontalTabStripScrollSource)
enum class HorizontalTabStripScrollSource {
  kButtons = 0,
  kTouchpadOrMouseWheel = 1,
  kMaxValue = kTouchpadOrMouseWheel,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/tab/enums.xml:HorizontalTabStripScrollSource)

void RecordHorizontalTabStripScrollSource(
    HorizontalTabStripScrollSource source);

void RecordHorizontalTabStripIsScrollable(bool is_scrollable);

// An event handler attached to the horizontal unpinned tabs ScrollView to log
// the TabStrip.Horizontal.ScrollSource metric when touchpad or mouse wheel
// scrolling occurs.
class UnpinnedTabScrollTypeRecorder : public ui::EventHandler {
 public:
  explicit UnpinnedTabScrollTypeRecorder(views::ScrollView* scroll_view);
  UnpinnedTabScrollTypeRecorder(const UnpinnedTabScrollTypeRecorder&) = delete;
  UnpinnedTabScrollTypeRecorder& operator=(
      const UnpinnedTabScrollTypeRecorder&) = delete;
  ~UnpinnedTabScrollTypeRecorder() override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  void MaybeLogScrollHistogram();

  raw_ptr<views::ScrollView> scroll_view_;
  base::TimeTicks last_scroll_time_;
};

// Periodically logs whether the unpinned tabs ScrollView is scrollable
// (i.e. content is overflowing).
class UnpinnedTabScrollableStateRecorder {
 public:
  explicit UnpinnedTabScrollableStateRecorder(views::ScrollView* scroll_view);
  UnpinnedTabScrollableStateRecorder(
      const UnpinnedTabScrollableStateRecorder&) = delete;
  UnpinnedTabScrollableStateRecorder& operator=(
      const UnpinnedTabScrollableStateRecorder&) = delete;
  ~UnpinnedTabScrollableStateRecorder();

  void SetTaskRunnerForTesting(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

 private:
  void RecordScrollableMetric();

  raw_ptr<views::ScrollView> scroll_view_;
  base::RepeatingTimer timer_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_HORIZONTAL_TAB_STRIP_METRICS_H_
