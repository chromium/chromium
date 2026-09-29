// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DICTATION_REVIEWING_PAGE_STATUS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DICTATION_REVIEWING_PAGE_STATUS_VIEW_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

namespace views {
class Label;
class Separator;
}  // namespace views

namespace dictation {

// A transient status shown in the dictation bubble at the start of a session
// to indicate that the page contents are being reviewed (i.e. shared as
// context).
//
// Once shown, the view auto-dismisses after a timeout. The timer is paused
// while the bubble is hovered or focused and resumes with the remaining time
// after it is no longer hovered or focused. `visibility_changed_callback` is
// run whenever the view is shown or dismissed so the owner can update the rest
// of the bubble layout.
class ReviewingPageStatusView : public views::View,
                                public views::FocusChangeListener {
  METADATA_HEADER(ReviewingPageStatusView, views::View)
 public:
  using VisibilityChangedCallback = base::RepeatingCallback<void(bool visible)>;

  explicit ReviewingPageStatusView(
      VisibilityChangedCallback visibility_changed_callback);
  ReviewingPageStatusView(const ReviewingPageStatusView&) = delete;
  ReviewingPageStatusView& operator=(const ReviewingPageStatusView&) = delete;
  ~ReviewingPageStatusView() override;

  void Init();
  void Show();
  void UpdateTimer();
  void SetDurationForTesting(base::TimeDelta duration);

  // views::View:
  void AddedToWidget() override;
  void RemovedFromWidget() override;

  // views::FocusChangeListener:
  void OnWillChangeFocus(views::View* focused_before,
                         views::View* focused_now) override {}
  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override;

 private:
  void Dismiss();
  bool HasFocusOrHover() const;

  VisibilityChangedCallback visibility_changed_callback_;
  base::OneShotTimer timer_;
  base::TimeDelta duration_;
  base::TimeDelta remaining_duration_;
  base::TimeTicks timer_start_time_;

  raw_ptr<views::Separator> separator_ = nullptr;
  raw_ptr<views::Label> label_ = nullptr;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_UI_VIEWS_DICTATION_REVIEWING_PAGE_STATUS_VIEW_H_
