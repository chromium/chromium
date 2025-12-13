// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORD_CHANGE_UI_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORD_CHANGE_UI_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/password_manager/password_change_delegate.h"
#include "chrome/browser/ui/views/passwords/password_change/password_change_toast.h"
#include "ui/views/widget/widget.h"

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace ui {
class DialogModel;
}  // namespace ui

// Possible actions that the user can take in dialogs displayed by this
// controller. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused.
// LINT.IfChange(PasswordChangeDialogAction)
enum class PasswordChangeDialogAction {
  kCancelButtonClicked = 0,
  kAcceptButtonClicked = 1,
  kLinkClicked = 2,
  kMaxValue = kLinkClicked,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/password/enums.xml:PasswordChangeDialogAction)

// Events happening for toasts displayed by this controller. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
// LINT.IfChange(PasswordChangeToastEvent)
enum class PasswordChangeToastEvent {
  kShown = 0,
  kCanceled = 1,
  kContinue = 2,
  kOpenPasswordChangeTab = 3,
  kRetry = 4,
  kMaxValue = kRetry,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/password/enums.xml:PasswordChangeToastEvent)

// Responsible for creating and displaying appropriate views based on the
// current state of the password change flow.
class PasswordChangeUIController {
 public:
  PasswordChangeUIController(PasswordChangeDelegate* password_change_delegate,
                             tabs::TabInterface* tab_interface);
  virtual ~PasswordChangeUIController();

  // Updates the `state_` and the UI.
  virtual void UpdateState(PasswordChangeDelegate::State state);

#if defined(UNIT_TEST)
  const views::Widget* dialog_widget() const { return dialog_widget_.get(); }
  PasswordChangeToast* toast_view() const { return toast_view_; }
  void CallOnDialogCanceledForTesting() { OnDialogCanceled(); }
#endif

 private:
  std::variant<PasswordChangeToast::ToastOptions,
               std::unique_ptr<ui::DialogModel>>
  GetDialogOrToastConfiguration(PasswordChangeDelegate::State state);

  void ShowToast(PasswordChangeToast::ToastOptions options);
  void ShowDialog(std::unique_ptr<ui::DialogModel> dialog_model);

  void OnToastCanceled();
  void OnDialogCanceled();
  void OpenPasswordChangeTab();
  void OpenPasswordChangeTabFromDialog();
  void OpenPasswordChangeTabFromToast();
  void StartPasswordChangeFlow();
  void OnPrivacyNoticeAccepted();
  void ShowPasswordDetails();
  void NavigateToPasswordChangeSettings();
  void RetryLoginCheck();

  // Closes the dialog or widget and logs the `reason`.
  // TODO(crbug.com/407504591): Actually log the reason.
  void CloseDialogWidget(views::Widget::ClosedReason reason);
  void CloseToastWidget(views::Widget::ClosedReason reason);

  // Controls password change process. Owns this class.
  const raw_ptr<PasswordChangeDelegate> password_change_delegate_;

  // A tab where a toast and a modal dialog is displayed.
  const raw_ptr<tabs::TabInterface> tab_interface_;

  // View displaying the progress of password change.
  raw_ptr<PasswordChangeToast> toast_view_;

  // Delegate for the `toast_widget_`.
  std::unique_ptr<views::WidgetDelegate> toast_delegate_;

  // Widget containing the currently open toast, if any.
  std::unique_ptr<views::Widget> toast_widget_;

  // Widget containing the currently open dialog, if any.
  std::unique_ptr<views::Widget> dialog_widget_;

  // Current state of the password change flow.
  PasswordChangeDelegate::State state_ =
      static_cast<PasswordChangeDelegate::State>(-1);

  base::WeakPtrFactory<PasswordChangeUIController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORD_CHANGE_UI_CONTROLLER_H_
