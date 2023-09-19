// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/plus_addresses/plus_address_creation_dialog_view.h"

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/widget/widget.h"

namespace plus_addresses {

// static
// TODO(crbug.com/1467623): Flesh out the modal content, etc.
void ShowPlusAddressCreationDialogView(
    content::WebContents* web_contents,
    base::WeakPtr<PlusAddressCreationController> controller) {
  CHECK(controller);

  auto dialog_model =
      ui::DialogModel::Builder(std::make_unique<ui::DialogModelDelegate>())
          .SetTitle(l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MODAL_TITLE))
          .AddOkButton(
              base::BindOnce(&PlusAddressCreationController::OnConfirmed,
                             controller),
              ui::DialogModelButton::Params().SetLabel(
                  l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MODAL_OK_TEXT)))
          .AddCancelButton(
              base::BindOnce(&PlusAddressCreationController::OnCanceled,
                             controller),
              ui::DialogModelButton::Params().SetLabel(
                  l10n_util::GetStringUTF16(
                      IDS_PLUS_ADDRESS_MODAL_CANCEL_TEXT)))
          .SetDialogDestroyingCallback(base::BindOnce(
              &PlusAddressCreationController::OnDialogDestroyed, controller))
          .Build();
  constrained_window::ShowWebModal(std::move(dialog_model), web_contents);
}

}  // namespace plus_addresses
