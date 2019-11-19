// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APP_MODAL_VIEWS_JAVASCRIPT_APP_MODAL_DIALOG_VIEWS_H_
#define COMPONENTS_APP_MODAL_VIEWS_JAVASCRIPT_APP_MODAL_DIALOG_VIEWS_H_

#include <memory>

#include "base/macros.h"
#include "components/app_modal/native_app_modal_dialog.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class MessageBoxView;
}

namespace app_modal {

class JavaScriptAppModalDialog;

class JavaScriptAppModalDialogViews : public NativeAppModalDialog,
                                      public views::DialogDelegate {
 public:
  explicit JavaScriptAppModalDialogViews(JavaScriptAppModalDialog* parent);
  ~JavaScriptAppModalDialogViews() override;

  // Overridden from NativeAppModalDialog:
  int GetAppModalDialogButtons() const override;
  void ShowAppModalDialog() override;
  void ActivateAppModalDialog() override;
  void CloseAppModalDialog() override;
  void AcceptAppModalDialog() override;
  void CancelAppModalDialog() override;
  bool IsShowing() const override;

  // Overridden from views::DialogDelegate:
  int GetDialogButtons() const override;
  base::string16 GetWindowTitle() const override;
  void DeleteDelegate() override;
  bool Cancel() override;
  bool Accept() override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;

  // Overridden from views::WidgetDelegate:
  ui::ModalType GetModalType() const override;
  views::View* GetContentsView() override;
  views::View* GetInitiallyFocusedView() override;
  bool ShouldShowCloseButton() const override;
  void WindowClosing() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;

 private:
  // A pointer to the AppModalDialog that owns us.
  std::unique_ptr<JavaScriptAppModalDialog> parent_;

  // The message box view whose commands we handle.
  views::MessageBoxView* message_box_view_;

  DISALLOW_COPY_AND_ASSIGN(JavaScriptAppModalDialogViews);
};

}  // namespace app_modal

#endif  // COMPONENTS_APP_MODAL_VIEWS_JAVASCRIPT_APP_MODAL_DIALOG_VIEWS_H_
