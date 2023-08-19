// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_POLICY_IDLE_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_POLICY_IDLE_DIALOG_VIEW_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/idle_dialog.h"
#include "chrome/browser/ui/views/relaunch_notification/relaunch_required_timer.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

namespace policy {

// A View for the idle timeout dialog. This is shown to users to inform them
// that Chrome will be closed by the IdleService, as dictated by the
// IdleProfileCloseTimeout policy.
class IdleDialogView : public views::DialogDelegateView {
 public:
  IdleDialogView(base::TimeDelta dialog_duration,
                 base::TimeDelta idle_threshold,
                 IdleDialog::ActionSet actions,
                 base::OnceClosure on_close_by_user);

  // Shows the dialog informing the user that Chrome will close after
  // |dialog_duration|. |idle_threshold| is the value of the
  // IdleProfileCloseTimeout policy, for displaying to the user.
  // |on_close_by_user| is run if the user clicks on "Continue", or presses
  // Escape to close the dialog.
  static base::WeakPtr<views::Widget> Show(Browser* browser,
                                           base::TimeDelta dialog_duration,
                                           base::TimeDelta idle_threshold,
                                           IdleDialog::ActionSet actions,
                                           base::OnceClosure on_close_by_user);

  IdleDialogView(const IdleDialogView&) = delete;
  IdleDialogView& operator=(const IdleDialogView&) = delete;

  ~IdleDialogView() override;

  // views::DialogDelegateView:
  std::u16string GetWindowTitle() const override;
  ui::ImageModel GetWindowIcon() override;

 private:
  // Updates the text in the dialog. Runs every second via
  // |update_timer_|.
  void UpdateCountdown();

  raw_ptr<views::Label> main_label_ = nullptr;
  raw_ptr<views::Label> incognito_label_ = nullptr;
  raw_ptr<views::Label> countdown_label_ = nullptr;

  // Fires every 1s to update the countdown.
  base::RepeatingTimer update_timer_;

  // Data used to generate user-visible strings.
  const base::TimeDelta idle_threshold_;
  const IdleDialog::ActionSet actions_;
  const base::TimeTicks deadline_;

  // Number of Incognito windows open when the dialog was shown. Cached to avoid
  // iterating through BrowserList every 1s.
  int incognito_count_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_UI_VIEWS_POLICY_IDLE_DIALOG_VIEW_H_
