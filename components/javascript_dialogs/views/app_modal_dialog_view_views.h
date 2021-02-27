// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JAVASCRIPT_DIALOGS_VIEWS_APP_MODAL_DIALOG_VIEW_VIEWS_H_
#define COMPONENTS_JAVASCRIPT_DIALOGS_VIEWS_APP_MODAL_DIALOG_VIEW_VIEWS_H_

#include <memory>

#include "base/macros.h"
#include "components/javascript_dialogs/app_modal_dialog_view.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class MessageBoxView;
}

namespace javascript_dialogs {

class AppModalDialogController;

class AppModalDialogViewViews : public AppModalDialogView,
                                public views::DialogDelegate {
 public:
  explicit AppModalDialogViewViews(AppModalDialogController* controller);
  ~AppModalDialogViewViews() override;

  // AppModalDialogView:
  void ShowAppModalDialog() override;
  void ActivateAppModalDialog() override;
  void CloseAppModalDialog() override;
  void AcceptAppModalDialog() override;
  void CancelAppModalDialog() override;
  bool IsShowing() const override;

  // views::DialogDelegate:
  base::string16 GetWindowTitle() const override;
  ui::ModalType GetModalType() const override;
  views::View* GetContentsView() override;
  views::View* GetInitiallyFocusedView() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  bool ShouldShowCloseButton() const override;
  void WindowClosing() override;

 private:
  std::unique_ptr<AppModalDialogController> controller_;

  // The message box view whose commands we handle.
  views::MessageBoxView* message_box_view_;

  DISALLOW_COPY_AND_ASSIGN(AppModalDialogViewViews);
};

}  // namespace javascript_dialogs

#endif  // COMPONENTS_JAVASCRIPT_DIALOGS_VIEWS_APP_MODAL_DIALOG_VIEW_VIEWS_H_
