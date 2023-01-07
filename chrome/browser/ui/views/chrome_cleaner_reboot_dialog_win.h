// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CHROME_CLEANER_REBOOT_DIALOG_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_CHROME_CLEANER_REBOOT_DIALOG_WIN_H_

#include "ui/base/metadata/metadata_header_macros.h"
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
  METADATA_HEADER(ChromeCleanerRebootDialog);
  // The |dialog_controller| object manages its own lifetime and is not owned
  // by |ChromeCleanerRebootDialog|. See the description of the
  // |ChromeCleanerRebootDialogController| class for details.
  explicit ChromeCleanerRebootDialog(
      safe_browsing::ChromeCleanerRebootDialogController* dialog_controller);
  ChromeCleanerRebootDialog(const ChromeCleanerRebootDialog&) = delete;
  ChromeCleanerRebootDialog& operator=(const ChromeCleanerRebootDialog&) =
      delete;
  ~ChromeCleanerRebootDialog() override;

  void Show(Browser* browser);

  // views::WidgetDelegate overrides.
  std::u16string GetWindowTitle() const override;
  views::View* GetInitiallyFocusedView() override;

 private:
  gfx::Rect GetDialogBounds(Browser* browser) const;

  // The pointer will be set to nullptr once the controller has been notified of
  // user interaction since the controller can delete itself after that point.
  safe_browsing::ChromeCleanerRebootDialogController* dialog_controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CHROME_CLEANER_REBOOT_DIALOG_WIN_H_
