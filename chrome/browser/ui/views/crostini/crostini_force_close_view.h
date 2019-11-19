// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_FORCE_CLOSE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_FORCE_CLOSE_VIEW_H_

#include "base/callback.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {
class Widget;
}

// Displays a dialog that allows the user to force close an associated widget
// via CloseNow().
class CrostiniForceCloseView : public views::BubbleDialogDelegateView {
 public:
  // Show the "would you like to force-close |app_name|?" dialog, which invokes
  // the |force_close_callback_| if the user chooses to force close. Returns the
  // widget for the force-close dialog. The |cloasble_widget| is used to decide
  // where to draw the dialog.
  static views::Widget* Show(const std::string& app_name,
                             views::Widget* closable_widget,
                             base::OnceClosure force_close_callback);

  // Similar to the above, but allowing direct use of the native view/window
  // which we need to decide how to place the dialog.
  static views::Widget* Show(const std::string& app_name,
                             gfx::NativeWindow closable_window,
                             gfx::NativeView closable_view,
                             base::OnceClosure force_close_callback);

  // BubbleDialogDelegateView overrides.
  bool Accept() override;
  ui::ModalType GetModalType() const override;
  bool ShouldShowCloseButton() const override;
  int GetDialogButtons() const override;
  base::string16 GetWindowTitle() const override;
  gfx::Size CalculatePreferredSize() const override;

 private:
  CrostiniForceCloseView(const std::string& app_name,
                         base::OnceClosure force_close_callback);

  ~CrostiniForceCloseView() override;

  // Then name of this application which we will display to the user. If we
  // don't know the app's name, this will be the empty string.
  base::string16 app_name_;

  // The callback to invoke if the user chooses to force close.
  base::OnceClosure force_close_callback_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniForceCloseView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_FORCE_CLOSE_VIEW_H_
