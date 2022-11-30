// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CRITICAL_NOTIFICATION_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CRITICAL_NOTIFICATION_BUBBLE_VIEW_H_

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class CriticalNotificationBubbleView : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(CriticalNotificationBubbleView);
  explicit CriticalNotificationBubbleView(views::View* anchor_view);
  CriticalNotificationBubbleView(const CriticalNotificationBubbleView&) =
      delete;
  CriticalNotificationBubbleView& operator=(
      const CriticalNotificationBubbleView&) = delete;
  ~CriticalNotificationBubbleView() override;

  // views::BubbleDialogDelegateView overrides:
  std::u16string GetWindowTitle() const override;
  void WindowClosing() override;
  void Init() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;

 private:
  // Helper function to calculate the remaining time until spontaneous reboot.
  base::TimeDelta GetRemainingTime() const;

  // Called when the timer fires each time the clock ticks.
  void OnCountdown();

  void OnDialogAccepted();
  void OnDialogCancelled();

  // A timer to refresh the bubble to show new countdown value.
  base::RepeatingTimer refresh_timer_;

  // When the bubble was created.
  base::TimeTicks bubble_created_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CRITICAL_NOTIFICATION_BUBBLE_VIEW_H_
