// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/delete_address_profile_dialog_view.h"

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/delete_address_profile_dialog_controller.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

namespace autofill {

// static
void ShowDeleteAddressProfileDialogView(
    content::WebContents* web_contents,
    base::WeakPtr<DeleteAddressProfileDialogController> controller) {
  DCHECK(controller);

  auto dialog_model =
      ui::DialogModel::Builder(std::make_unique<ui::DialogModelDelegate>())
          .SetTitle(controller->GetTitle())
          .AddOkButton(
              base::BindOnce(&DeleteAddressProfileDialogController::OnAccepted,
                             controller),
              ui::DialogModel::Button::Params()
                  .SetId(views::DialogClientView::kOkButtonElementId)
                  .SetLabel(controller->GetAcceptButtonText()))
          .AddCancelButton(
              base::BindOnce(&DeleteAddressProfileDialogController::OnCanceled,
                             controller),
              ui::DialogModel::Button::Params()
                  .SetId(views::DialogClientView::kCancelButtonElementId)
                  .SetLabel(controller->GetDeclineButtonText()))
          .AddParagraph(
              ui::DialogModelLabel(controller->GetDeleteConfirmationText())
                  .set_is_secondary())
          .SetCloseActionCallback(base::BindOnce(
              &DeleteAddressProfileDialogController::OnClosed, controller))
          .SetDialogDestroyingCallback(base::BindOnce(
              &DeleteAddressProfileDialogController::OnDialogDestroying,
              controller))
          .Build();

  chrome::ShowTabModal(std::move(dialog_model), web_contents);
}

}  // namespace autofill
