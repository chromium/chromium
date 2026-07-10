// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/groups/tab_group_editor_bubble_tracker.h"

#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/widget/widget.h"

namespace {
constexpr base::TimeDelta kIgnoreScrollDuration = base::Milliseconds(250);
}  // namespace

TabGroupEditorBubbleTracker::TabGroupEditorBubbleTracker(
    tabs::VerticalTabStripStateController* state_controller) {
  if (state_controller) {
    vertical_tab_mode_will_change_subscription_ =
        state_controller->RegisterOnModeWillChange(base::BindRepeating(
            &TabGroupEditorBubbleTracker::OnVerticalTabStripModeWillChange,
            base::Unretained(this)));
  }
}

TabGroupEditorBubbleTracker::~TabGroupEditorBubbleTracker() {
  is_open_ = false;
  weak_ptr_factory_.InvalidateWeakPtrs();
  widget_.reset();
}

void TabGroupEditorBubbleTracker::SetScrollView(
    views::ScrollView* scroll_view) {
  scroll_view_subscription_ =
      scroll_view
          ? scroll_view->AddContentsScrolledCallback(base::BindRepeating(
                &TabGroupEditorBubbleTracker::OnContentsScrolled,
                base::Unretained(this)))
          : base::CallbackListSubscription();
}

void TabGroupEditorBubbleTracker::Opened(
    std::unique_ptr<views::Widget> bubble_widget) {
  DCHECK(bubble_widget);
  DCHECK(!is_open_);
  widget_ = std::move(bubble_widget);
  is_open_ = true;
  widget_->MakeCloseSynchronous(
      base::BindOnce(&TabGroupEditorBubbleTracker::CloseWidget,
                     weak_ptr_factory_.GetWeakPtr()));
  // Ignore scroll events for a short duration after the bubble is opened.
  // This prevents the bubble from closing prematurely when it is opened
  // during group creation and the group is automatically scrolled into view,
  // which triggers a scroll event.
  ignore_scroll_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TabGroupEditorBubbleTracker::StopIgnoringScroll,
                     weak_ptr_factory_.GetWeakPtr()),
      kIgnoreScrollDuration);
  on_bubble_opened_callback_list_.Notify();
}

base::CallbackListSubscription
TabGroupEditorBubbleTracker::RegisterOnBubbleOpened(
    base::RepeatingCallback<void()> callback) {
  return on_bubble_opened_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription
TabGroupEditorBubbleTracker::RegisterOnBubbleClosed(
    base::RepeatingCallback<void()> callback) {
  return on_bubble_closed_callback_list_.Add(std::move(callback));
}

void TabGroupEditorBubbleTracker::CloseWidget(
    views::Widget::ClosedReason reason) {
  if (!is_open_) {
    return;
  }

  is_open_ = false;

  if (widget_) {
    widget_->CloseWithReason(reason);
  }

  // The widget cannot be destroyed synchronously here because CloseWidget is
  // often called from within a Widget observer iteration (e.g., inside
  // OnWidgetActivationChanged). Doing so would destroy the observer list.
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, widget_.release());

  on_bubble_closed_callback_list_.Notify();
}

void TabGroupEditorBubbleTracker::OnVerticalTabStripModeWillChange(
    tabs::VerticalTabStripStateController* controller) {
  CloseWidget(views::Widget::ClosedReason::kUnspecified);
}

void TabGroupEditorBubbleTracker::OnContentsScrolled() {
  if (ignore_scroll_) {
    return;
  }
  CloseWidget(views::Widget::ClosedReason::kUnspecified);
}

void TabGroupEditorBubbleTracker::StopIgnoringScroll() {
  ignore_scroll_ = false;
}
