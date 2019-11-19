// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CHROME_CLEANER_REBOOT_DIALOG_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_CHROME_CLEANER_REBOOT_DIALOG_WIN_H_

#include "base/macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/window/dialog_delegate.h"

class Browser;

namespace safe_browsing {
class ChromeCleanerRebootDialogController;
}

// A modal dialog asking the user if they want to restart their computer once
// the Chrome Cleanup tool finished in a reboot required state.
//
// A ChromeCleanerRebootDialogController object will receive information
// about how the user interacts with the dialog. The controller object owns
// itself and will delete itself once it has received information about the
// user's interaction with the dialog.
class ChromeCleanerRebootDialog : public views::DialogDelegateView {
 public:
  // The |dialog_controller| object manages its own lifetime and is not owned
  // by |ChromeCleanerRebootDialog|. See the description of the
  // |ChromeCleanerRebootDialogController| class for details.
  explicit ChromeCleanerRebootDialog(
      safe_browsing::ChromeCleanerRebootDialogController* dialog_controller);
  ~ChromeCleanerRebootDialog() override;

  void Show(Browser* browser);

  // views::WidgetDelegate overrides.
  ui::ModalType GetModalType() const override;
  base::string16 GetWindowTitle() const override;
  views::View* GetInitiallyFocusedView() override;

  // views::DialogDelegate overrides.
  bool Accept() override;
  bool Cancel() override;
  bool Close() override;

 private:
  enum class DialogInteractionResult {
    kAccept,
    kCancel,
    kClose,
    kClosedOnDestruction,
  };

  void HandleDialogInteraction(DialogInteractionResult result);

  gfx::Rect GetDialogBounds(Browser* browser) const;

  // The pointer will be set to nullptr once the controller has been notified of
  // user interaction since the controller can delete itself after that point.
  safe_browsing::ChromeCleanerRebootDialogController* dialog_controller_;

  DISALLOW_COPY_AND_ASSIGN(ChromeCleanerRebootDialog);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CHROME_CLEANER_REBOOT_DIALOG_WIN_H_
