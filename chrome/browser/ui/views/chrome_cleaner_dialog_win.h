// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CHROME_CLEANER_DIALOG_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_CHROME_CLEANER_DIALOG_WIN_H_

#include <memory>
#include <set>

#include "base/macros.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_controller_win.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/window/dialog_delegate.h"

class Browser;

namespace safe_browsing {
class ChromeCleanerDialogController;
}

namespace views {
class Checkbox;
class LabelButton;
}  // namespace views

// A modal dialog asking the user if they want to remove harmful software from
// their computers by running the Chrome Cleanup tool.
//
// The strings and icons used in the dialog are provided by a
// |ChromeCleanerDialogController| object, which will also receive information
// about how the user interacts with the dialog. The controller object owns
// itself and will delete itself once it has received information about the
// user's interaction with the dialog. See the |ChromeCleanerDialogController|
// class's description for more details.
class ChromeCleanerDialog
    : public views::DialogDelegateView,
      public views::ButtonListener,
      public safe_browsing::ChromeCleanerController::Observer {
 public:
  // The |controller| object manages its own lifetime and is not owned by
  // |ChromeCleanerDialog|. See the description of the
  // |ChromeCleanerDialogController| class for details.
  ChromeCleanerDialog(
      safe_browsing::ChromeCleanerDialogController* dialog_controller,
      safe_browsing::ChromeCleanerController* cleaner_controller);
  ~ChromeCleanerDialog() override;

  void Show(Browser* browser);

  // views::WidgetDelegate overrides.
  ui::ModalType GetModalType() const override;
  base::string16 GetWindowTitle() const override;
  views::View* GetInitiallyFocusedView() override;
  bool ShouldShowCloseButton() const override;

  // views::DialogDelegate overrides.
  bool Accept() override;
  bool Cancel() override;
  bool Close() override;

  // views::View overrides.
  gfx::Size CalculatePreferredSize() const override;

  // views::ButtonListener overrides.
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // safe_browsing::ChromeCleanerController::Observer overrides.
  void OnIdle(
      safe_browsing::ChromeCleanerController::IdleReason idle_reason) override;
  void OnScanning() override;
  void OnCleaning(bool is_powered_by_partner,
                  const safe_browsing::ChromeCleanerScannerResults&
                      scanner_results) override;
  void OnRebootRequired() override;

 private:
  enum class DialogInteractionResult;

  void HandleDialogInteraction(DialogInteractionResult result);
  void Abort();

  Browser* browser_ = nullptr;
  // The pointer will be set to nullptr once the controller has been notified of
  // user interaction since the controller can delete itself after that point.
  safe_browsing::ChromeCleanerDialogController* dialog_controller_ = nullptr;
  safe_browsing::ChromeCleanerController* cleaner_controller_ = nullptr;
  views::LabelButton* details_button_ = nullptr;
  views::Checkbox* logs_permission_checkbox_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ChromeCleanerDialog);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CHROME_CLEANER_DIALOG_WIN_H_
