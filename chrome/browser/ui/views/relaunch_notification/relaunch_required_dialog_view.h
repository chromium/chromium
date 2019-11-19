// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_REQUIRED_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_REQUIRED_DIALOG_VIEW_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/relaunch_notification/relaunch_required_timer.h"
#include "ui/views/window/dialog_delegate.h"

class Browser;
namespace views {
class Widget;
}  // namespace views

// A View for the relaunch required dialog. This is shown to users to inform
// them that Chrome will be relaunched by the RelaunchNotificationController as
// dictated by policy settings and upgrade availability.
class RelaunchRequiredDialogView : views::DialogDelegateView {
 public:
  // Shows the dialog in |browser| for a relaunch that will be forced at
  // |deadline|. |on_accept| is run if the user accepts the prompt to restart.
  static views::Widget* Show(Browser* browser,
                             base::Time deadline,
                             base::RepeatingClosure on_accept);

  ~RelaunchRequiredDialogView() override;

  // Returns the instance hosted by |widget|. |widget| must be an instance
  // previously returned from Show().
  static RelaunchRequiredDialogView* FromWidget(views::Widget* widget);

  // Sets the relaunch deadline to |deadline| and refreshes the view's title
  // accordingly.
  void SetDeadline(base::Time deadline);

  // views::DialogDelegateView:
  bool Cancel() override;
  bool Accept() override;
  ui::ModalType GetModalType() const override;
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  gfx::ImageSkia GetWindowIcon() override;
  bool ShouldShowWindowIcon() const override;

 protected:
  // views::DialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;

 private:
  RelaunchRequiredDialogView(base::Time deadline,
                             base::RepeatingClosure on_accept);

  // Invoked when the timer fires to refresh the title text.
  void UpdateWindowTitle();

  static constexpr int kTitleIconSize = 20;

  // A callback to run if the user accepts the prompt to relaunch the browser.
  base::RepeatingClosure on_accept_;

  // Timer that schedules title refreshes.
  RelaunchRequiredTimer relaunch_required_timer_;

  DISALLOW_COPY_AND_ASSIGN(RelaunchRequiredDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_REQUIRED_DIALOG_VIEW_H_
