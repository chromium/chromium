// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_POLICY_ENTERPRISE_STARTUP_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_POLICY_ENTERPRISE_STARTUP_DIALOG_VIEW_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/enterprise_startup_dialog.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

namespace policy {

class EnterpriseStartupDialogView : public views::DialogDelegateView {
 public:
  EnterpriseStartupDialogView(
      EnterpriseStartupDialog::DialogResultCallback callback);
  ~EnterpriseStartupDialogView() override;

  void DisplayLaunchingInformationWithThrobber(
      const base::string16& information);
  void DisplayErrorMessage(const base::string16& error_message,
                           const base::Optional<base::string16>& accept_button);
  void CloseDialog();

  void AddWidgetObserver(views::WidgetObserver* observer);
  void RemoveWidgetObserver(views::WidgetObserver* observer);

 private:
  // Run the dialog modally for MacOSX.
  void StartModalDialog();

  // Call the dialog callback. |was_accepted| is true if the dialog is confirmed
  // by user. Otherwise it's false.
  void RunDialogCallback(bool was_accepted);

  // override views::DialogDelegateView
  bool ShouldShowWindowTitle() const override;
  ui::ModalType GetModalType() const override;

  // override views::View
  gfx::Size CalculatePreferredSize() const override;

  // Remove all existing child views from the dialog, show/hide dialog buttons.
  void ResetDialog(bool show_accept_button);
  // Append child views to the content area, setup the layout.
  void SetupLayout(std::unique_ptr<views::View> icon,
                   std::unique_ptr<views::View> text);

  EnterpriseStartupDialog::DialogResultCallback callback_;
  bool can_show_browser_window_ = false;

  base::WeakPtrFactory<EnterpriseStartupDialogView> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(EnterpriseStartupDialogView);
};

class EnterpriseStartupDialogImpl : public EnterpriseStartupDialog,
                                    public views::WidgetObserver {
 public:
  explicit EnterpriseStartupDialogImpl(DialogResultCallback callback);
  ~EnterpriseStartupDialogImpl() override;

  // Override EnterpriseStartupDialog
  void DisplayLaunchingInformationWithThrobber(
      const base::string16& information) override;
  void DisplayErrorMessage(
      const base::string16& error_message,
      const base::Optional<base::string16>& accept_button) override;
  bool IsShowing() override;

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;

 private:
  // The dialog_view_ is owned by itself.
  EnterpriseStartupDialogView* dialog_view_;

  DISALLOW_COPY_AND_ASSIGN(EnterpriseStartupDialogImpl);
};

}  // namespace policy
#endif  // CHROME_BROWSER_UI_VIEWS_POLICY_ENTERPRISE_STARTUP_DIALOG_VIEW_H_
