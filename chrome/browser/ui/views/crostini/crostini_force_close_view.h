// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_FORCE_CLOSE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_FORCE_CLOSE_VIEW_H_

#include "base/functional/callback.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

#include <string>

namespace views {
class Widget;
}

namespace crostini {

// Shows the Crostini force-close dialog. If |app_name| is nonempty, the dialog
// will include the window's name as text. Returns a handle to that dialog, so
// that we can add observers to the dialog itself.
views::Widget* ShowCrostiniForceCloseDialog(
    const std::string& app_name,
    views::Widget* closable_widget,
    base::OnceClosure force_close_callback);

}  // namespace crostini

// Displays a dialog that allows the user to force close an associated widget
// via CloseNow().
class CrostiniForceCloseView : public views::BubbleDialogDelegateView {
  METADATA_HEADER(CrostiniForceCloseView, views::BubbleDialogDelegateView)

 public:
  CrostiniForceCloseView(const CrostiniForceCloseView&) = delete;
  CrostiniForceCloseView& operator=(const CrostiniForceCloseView&) = delete;

  // Show the "would you like to force-close |app_name|?" dialog, which invokes
  // the |force_close_callback_| if the user chooses to force close. Returns the
  // widget for the force-close dialog. The |closable_widget| will be used as
  // the parent window for the dialog.
  static views::Widget* Show(const std::string& app_name,
                             views::Widget* closable_widget,
                             base::OnceClosure force_close_callback);

  // Similar to the above, but allowing direct use of the native view/window
  // which we need to decide how to place the dialog.
  static views::Widget* Show(const std::string& app_name,
                             gfx::NativeWindow context,
                             gfx::NativeView parent,
                             base::OnceClosure force_close_callback);

 private:
  CrostiniForceCloseView(const std::u16string& app_name,
                         base::OnceClosure force_close_callback);

  ~CrostiniForceCloseView() override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_FORCE_CLOSE_VIEW_H_
