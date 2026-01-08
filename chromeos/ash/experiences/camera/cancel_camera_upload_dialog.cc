// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/camera/cancel_camera_upload_dialog.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/time_formatting.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/widget/widget_delegate.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(CancelCameraUploadDialog,
                                      kSkipDialogCheckboxId);

CancelCameraUploadDialog::~CancelCameraUploadDialog() {
  if (widget_) {
    widget_->CloseNow();
  }
}

CancelCameraUploadDialog::CancelCameraUploadDialog(ClickedCallback callback)
    : clicked_callback_(std::move(callback)) {
  gfx::NativeView parent =
      ash::Shell::GetContainer(ash::Shell::GetRootWindowForNewWindows(),
                               ash::kShellWindowId_SystemModalContainer);

  auto model_delegate = std::make_unique<ui::DialogModelDelegate>();
  auto* model_delegate_ptr = model_delegate.get();
  auto dialog_model =
      ui::DialogModel::Builder(std::move(model_delegate))
          .SetTitle(l10n_util::GetStringUTF16(
              IDS_POLICY_SKYVAULT_CAMERA_CANCEL_UPLOAD_TITLE))
          .AddParagraph(
              ui::DialogModelLabel(
                  l10n_util::GetStringUTF16(
                      IDS_POLICY_SKYVAULT_CAMERA_CANCEL_UPLOAD_MESSAGE))
                  .set_is_secondary())
          .AddCheckbox(
              kSkipDialogCheckboxId,
              ui::DialogModelLabel(l10n_util::GetStringUTF16(
                  IDS_POLICY_SKYVAULT_CAMERA_CANCEL_UPLOAD_SKIP_CHECKBOX)))
          .AddCancelButton(
              base::BindOnce(&CancelCameraUploadDialog::OnCloseClicked,
                             weak_ptr_factory_.GetWeakPtr(),
                             model_delegate_ptr),
              ui::DialogModel::Button::Params().SetLabel(
                  l10n_util::GetStringUTF16(
                      IDS_POLICY_SKYVAULT_CAMERA_CANCEL_UPLOAD_CLOSE_BUTTON)))
          .AddOkButton(
              base::BindOnce(&CancelCameraUploadDialog::OnCancelClicked,
                             weak_ptr_factory_.GetWeakPtr(),
                             model_delegate_ptr),
              ui::DialogModel::Button::Params().SetLabel(
                  l10n_util::GetStringUTF16(
                      IDS_POLICY_SKYVAULT_CAMERA_CANCEL_UPLOAD_CANCEL_BUTTON)))
          .Build();
  auto dialog_delegate = views::BubbleDialogModelHost::CreateModal(
      std::move(dialog_model), ui::mojom::ModalType::kSystem,
      /*autosize=*/false);
  widget_ = constrained_window::CreateBrowserModalDialogViews(
                std::move(dialog_delegate), parent)
                ->GetWeakPtr();
  widget_->Show();
}

void CancelCameraUploadDialog::OnCancelClicked(
    ui::DialogModelDelegate* model_delegate) {
  clicked_callback_.Run(true, model_delegate->dialog_model()
                                  ->GetCheckboxByUniqueId(kSkipDialogCheckboxId)
                                  ->is_checked());
}

void CancelCameraUploadDialog::OnCloseClicked(
    ui::DialogModelDelegate* model_delegate) {
  clicked_callback_.Run(false,
                        model_delegate->dialog_model()
                            ->GetCheckboxByUniqueId(kSkipDialogCheckboxId)
                            ->is_checked());
}
