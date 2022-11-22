// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_POLICY_IDLE_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_POLICY_IDLE_DIALOG_VIEW_H_

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/idle_dialog.h"
#include "chrome/browser/ui/views/relaunch_notification/relaunch_required_timer.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Widget;
}  // namespace views

namespace policy {

// A View for the idle timeout dialog. This is shown to users to inform them
// that Chrome will be closed by the IdleService, as dictated by the
// IdleProfileCloseTimeout policy.
class IdleDialogView : views::BubbleDialogDelegateView {
 public:
  // Shows the dialog informing the user that Chrome will close after
  // |dialog_duration|. |idle_threshold| is the value of the
  // IdleProfileCloseTimeout policy, for displaying to the user.
  // |on_close_by_user| is run if the user clicks on "Continue", or presses
  // Escape to close the dialog.
  static views::Widget* Show(base::TimeDelta dialog_duration,
                             base::TimeDelta idle_threshold,
                             base::RepeatingClosure on_close_by_user);

  IdleDialogView(const IdleDialogView&) = delete;
  IdleDialogView& operator=(const IdleDialogView&) = delete;

  ~IdleDialogView() override;

  // views::DialogDelegateView:
  std::u16string GetWindowTitle() const override;
  ui::ImageModel GetWindowIcon() override;

 private:
  IdleDialogView(base::TimeDelta dialog_duration,
                 base::TimeDelta idle_threshold,
                 base::RepeatingClosure on_close_by_user);

  // Updates the text in the dialog. Runs every second via
  // |update_timer_|.
  void UpdateBody();

  raw_ptr<views::Label> main_label_ = nullptr;
  raw_ptr<views::Label> incognito_label_ = nullptr;
  raw_ptr<views::Label> countdown_label_ = nullptr;

  // When |deadline_| is reached, this dialog will automatically close. Meant
  // for displaying to the user.
  const base::Time deadline_;

  // How much time it took for this dialog to trigger. Meant for displaying to
  // the user.
  const int minutes_;

  // Number of Incognito windows open when the dialog was shown. Cached to avoid
  // iterating through BrowserList every 1s.
  int incognito_count_;

  // Fires every 1s to update the countdown.
  base::RepeatingTimer update_timer_;
};

// Owns the IdleDialogView widget. Created via IdleDialog::Show().
class IdleDialogImpl : public IdleDialog, public views::WidgetObserver {
 public:
  explicit IdleDialogImpl(views::Widget* dialog);

  ~IdleDialogImpl() override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  raw_ptr<views::Widget> widget_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_UI_VIEWS_POLICY_IDLE_DIALOG_VIEW_H_
