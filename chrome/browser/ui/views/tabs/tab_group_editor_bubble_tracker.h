// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_EDITOR_BUBBLE_TRACKER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_EDITOR_BUBBLE_TRACKER_H_

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
}

// Tracks whether the editor bubble is open. At most one can be open
// at once.
class TabGroupEditorBubbleTracker : public views::WidgetObserver {
 public:
  TabGroupEditorBubbleTracker();
  ~TabGroupEditorBubbleTracker() override;

  void Opened(views::Widget* bubble_widget);
  bool is_open() const { return is_open_; }
  views::Widget* widget() const { return widget_; }

  base::CallbackListSubscription RegisterOnBubbleOpened(
      base::RepeatingCallback<void()> callback);
  base::CallbackListSubscription RegisterOnBubbleClosed(
      base::RepeatingCallback<void()> callback);

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  bool is_open_ = false;
  raw_ptr<views::Widget, AcrossTasksDanglingUntriaged> widget_;
  base::RepeatingCallbackList<void()> on_bubble_opened_callback_list_;
  base::RepeatingCallbackList<void()> on_bubble_closed_callback_list_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_EDITOR_BUBBLE_TRACKER_H_
