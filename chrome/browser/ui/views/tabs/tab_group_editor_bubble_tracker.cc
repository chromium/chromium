// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_group_editor_bubble_tracker.h"

#include "ui/views/widget/widget.h"

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
  if (is_open_ && widget_) {
    widget_->RemoveObserver(this);
    widget_->Close();
    on_bubble_closed_callback_list_.Notify();
  }
  CHECK(!IsInObserverList());
}

void TabGroupEditorBubbleTracker::Opened(views::Widget* bubble_widget) {
  DCHECK(bubble_widget);
  DCHECK(!is_open_);
  widget_ = bubble_widget;
  is_open_ = true;
  bubble_widget->AddObserver(this);
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

void TabGroupEditorBubbleTracker::OnWidgetDestroying(
    views::Widget* bubble_widget) {
  CHECK(widget_ == bubble_widget);
  is_open_ = false;
  widget_->RemoveObserver(this);
  widget_ = nullptr;
  on_bubble_closed_callback_list_.Notify();
}

void TabGroupEditorBubbleTracker::OnVerticalTabStripModeWillChange(
    tabs::VerticalTabStripStateController* controller) {
  if (widget_) {
    widget_->CloseNow();
  }
}
