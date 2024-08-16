// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JAVASCRIPT_DIALOGS_VIEWS_APP_MODAL_DIALOG_VIEW_VIEWS_H_
#define COMPONENTS_JAVASCRIPT_DIALOGS_VIEWS_APP_MODAL_DIALOG_VIEW_VIEWS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "components/javascript_dialogs/app_modal_dialog_view.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class MessageBoxView;
}

namespace javascript_dialogs {

class AppModalDialogController;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
class LayerDimmer;
#endif  // IS_CHROMEOS_LACROS

class AppModalDialogViewViews : public AppModalDialogView,
                                public views::DialogDelegate {
 public:
  explicit AppModalDialogViewViews(AppModalDialogController* controller);

  AppModalDialogViewViews(const AppModalDialogViewViews&) = delete;
  AppModalDialogViewViews& operator=(const AppModalDialogViewViews&) = delete;

  ~AppModalDialogViewViews() override;

  // AppModalDialogView:
  void ShowAppModalDialog() override;
  void ActivateAppModalDialog() override;
  void CloseAppModalDialog() override;
  void AcceptAppModalDialog() override;
  void CancelAppModalDialog() override;
  bool IsShowing() const override;

  // views::DialogDelegate:
  std::u16string GetWindowTitle() const override;
  ui::mojom::ModalType GetModalType() const override;
  views::View* GetContentsView() override;
  views::View* GetInitiallyFocusedView() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  bool ShouldShowCloseButton() const override;
  void WindowClosing() override;

 private:
  std::unique_ptr<AppModalDialogController> controller_;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::unique_ptr<LayerDimmer> layerDimmer_;
#endif  // IS_CHROMEOS_LACROS

  // The message box view whose commands we handle.
  raw_ptr<views::MessageBoxView> message_box_view_;
};

}  // namespace javascript_dialogs

#endif  // COMPONENTS_JAVASCRIPT_DIALOGS_VIEWS_APP_MODAL_DIALOG_VIEW_VIEWS_H_
