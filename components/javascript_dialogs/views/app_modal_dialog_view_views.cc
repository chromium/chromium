// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/javascript_dialogs/views/app_modal_dialog_view_views.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/javascript_dialogs/app_modal_dialog_controller.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "components/javascript_dialogs/views/layer_dimmer.h"
#include "ui/aura/window.h"
#endif  // IS_CHROMEOS_LACROS

namespace javascript_dialogs {

////////////////////////////////////////////////////////////////////////////////
// AppModalDialogViewViews, public:

AppModalDialogViewViews::AppModalDialogViewViews(
    AppModalDialogController* controller)
    : controller_(controller) {
  SetOwnedByWidget(true);
  message_box_view_ = new views::MessageBoxView(
      controller->message_text(), /* detect_directionality = */ true);

  if (controller->javascript_dialog_type() ==
      content::JAVASCRIPT_DIALOG_TYPE_PROMPT) {
    message_box_view_->SetPromptField(controller->default_prompt_text());
  }

  message_box_view_->AddAccelerator(
      ui::Accelerator(ui::VKEY_C, ui::EF_CONTROL_DOWN));
  if (controller->display_suppress_checkbox()) {
    message_box_view_->SetCheckBoxLabel(
        l10n_util::GetStringUTF16(IDS_JAVASCRIPT_MESSAGEBOX_SUPPRESS_OPTION));
  }

  DialogDelegate::SetButtons(
      controller_->javascript_dialog_type() ==
              content::JAVASCRIPT_DIALOG_TYPE_ALERT
          ? static_cast<int>(ui::mojom::DialogButton::kOk)
          : static_cast<int>(ui::mojom::DialogButton::kOk) |
                static_cast<int>(ui::mojom::DialogButton::kCancel));
  DialogDelegate::SetAcceptCallback(base::BindOnce(
      [](AppModalDialogViewViews* dialog) {
        dialog->controller_->OnAccept(
            dialog->message_box_view_->GetInputText(),
            dialog->message_box_view_->IsCheckBoxSelected());
      },
      base::Unretained(this)));
  auto cancel_callback = [](AppModalDialogViewViews* dialog) {
    dialog->controller_->OnCancel(
        dialog->message_box_view_->IsCheckBoxSelected());
  };
  DialogDelegate::SetCancelCallback(
      base::BindOnce(cancel_callback, base::Unretained(this)));
  DialogDelegate::SetCloseCallback(
      base::BindOnce(cancel_callback, base::Unretained(this)));

  if (controller_->is_before_unload_dialog()) {
    DialogDelegate::SetButtonLabel(
        ui::mojom::DialogButton::kOk,
        l10n_util::GetStringUTF16(
            controller_->is_reload()
                ? IDS_BEFORERELOAD_MESSAGEBOX_OK_BUTTON_LABEL
                : IDS_BEFOREUNLOAD_MESSAGEBOX_OK_BUTTON_LABEL));
  }
}

AppModalDialogViewViews::~AppModalDialogViewViews() = default;

////////////////////////////////////////////////////////////////////////////////
// AppModalDialogViewViews, AppModalDialogView implementation:

void AppModalDialogViewViews::ShowAppModalDialog() {
  auto* widget = GetWidget();
  widget->Show();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* dialogWindow = widget->GetNativeWindow();
  auto* parentWindow = dialogWindow->parent();

  if (!layerDimmer_) {
    layerDimmer_ = std::make_unique<LayerDimmer>(parentWindow, dialogWindow);
  }
  layerDimmer_->Show();
#endif  // IS_CHROMEOS_LACROS
}

void AppModalDialogViewViews::ActivateAppModalDialog() {
  GetWidget()->Show();
  GetWidget()->Activate();
}

void AppModalDialogViewViews::CloseAppModalDialog() {
  GetWidget()->Close();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (layerDimmer_) {
    layerDimmer_->Hide();
  }
#endif  // IS_CHROMEOS_LACROS
}

void AppModalDialogViewViews::AcceptAppModalDialog() {
  AcceptDialog();
}

void AppModalDialogViewViews::CancelAppModalDialog() {
  CancelDialog();
}

bool AppModalDialogViewViews::IsShowing() const {
  return GetWidget()->IsVisible();
}

//////////////////////////////////////////////////////////////////////////////
// AppModalDialogViewViews, views::DialogDelegate implementation:

std::u16string AppModalDialogViewViews::GetWindowTitle() const {
  return controller_->title();
}

ui::mojom::ModalType AppModalDialogViewViews::GetModalType() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(crbug.com/40148438): Remove this hack. This works around the
  // linked bug. This dialog should be window-modal on ChromeOS as well.
  return ui::mojom::ModalType::kSystem;
#else
  return ui::mojom::ModalType::kWindow;
#endif
}

views::View* AppModalDialogViewViews::GetContentsView() {
  return message_box_view_;
}

views::View* AppModalDialogViewViews::GetInitiallyFocusedView() {
  if (message_box_view_->GetVisiblePromptField())
    return message_box_view_->GetVisiblePromptField();
  return views::DialogDelegate::GetInitiallyFocusedView();
}

bool AppModalDialogViewViews::ShouldShowCloseButton() const {
  return false;
}

void AppModalDialogViewViews::WindowClosing() {
  controller_->OnClose();
}

views::Widget* AppModalDialogViewViews::GetWidget() {
  return message_box_view_->GetWidget();
}

const views::Widget* AppModalDialogViewViews::GetWidget() const {
  return message_box_view_->GetWidget();
}

}  // namespace javascript_dialogs
