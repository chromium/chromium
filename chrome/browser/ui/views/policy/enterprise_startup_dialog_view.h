// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_POLICY_ENTERPRISE_STARTUP_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_POLICY_ENTERPRISE_STARTUP_DIALOG_VIEW_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/enterprise_startup_dialog.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

namespace policy {

class EnterpriseStartupDialogView : public views::DialogDelegateView {
  METADATA_HEADER(EnterpriseStartupDialogView, views::DialogDelegateView)

 public:
  EnterpriseStartupDialogView(
      EnterpriseStartupDialog::DialogResultCallback callback);
  EnterpriseStartupDialogView(const EnterpriseStartupDialogView&) = delete;
  EnterpriseStartupDialogView& operator=(const EnterpriseStartupDialogView&) =
      delete;
  ~EnterpriseStartupDialogView() override;

  void DisplayLaunchingInformationWithThrobber(
      const std::u16string& information);
  void DisplayErrorMessage(const std::u16string& error_message,
                           const std::optional<std::u16string>& accept_button);
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

  // override views::View
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // Remove all existing child views from the dialog, show/hide dialog buttons.
  void ResetDialog(bool show_accept_button);
  // Append child views to the content area.
  void AddContent(std::unique_ptr<views::View> icon,
                  std::unique_ptr<views::View> text);

  EnterpriseStartupDialog::DialogResultCallback callback_;
  bool can_show_browser_window_ = false;

  base::WeakPtrFactory<EnterpriseStartupDialogView> weak_factory_{this};
};

class EnterpriseStartupDialogImpl : public EnterpriseStartupDialog,
                                    public views::WidgetObserver {
 public:
  explicit EnterpriseStartupDialogImpl(DialogResultCallback callback);
  EnterpriseStartupDialogImpl(const EnterpriseStartupDialogImpl&) = delete;
  EnterpriseStartupDialogImpl& operator=(const EnterpriseStartupDialogImpl&) =
      delete;
  ~EnterpriseStartupDialogImpl() override;

  // Override EnterpriseStartupDialog
  void DisplayLaunchingInformationWithThrobber(
      const std::u16string& information) override;
  void DisplayErrorMessage(
      const std::u16string& error_message,
      const std::optional<std::u16string>& accept_button) override;
  bool IsShowing() override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  // The dialog_view_ is owned by itself.
  raw_ptr<EnterpriseStartupDialogView> dialog_view_;
};

}  // namespace policy
#endif  // CHROME_BROWSER_UI_VIEWS_POLICY_ENTERPRISE_STARTUP_DIALOG_VIEW_H_
