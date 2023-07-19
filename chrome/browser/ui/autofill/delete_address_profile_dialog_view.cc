// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/delete_address_profile_dialog_view.h"

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/delete_address_profile_dialog_controller.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/widget/widget.h"

namespace autofill::dialogs {

// static
// TODO(crbug.com/1459990): Remove hard coded strings and use email address to
// identify account from where we are deleting the address profile from.
views::Widget* ShowDeleteAddressProfileDialogView(
    content::WebContents* web_contents,
    base::WeakPtr<DeleteAddressProfileDialogController> controller) {
  DCHECK(controller);

  auto dialog_model =
      ui::DialogModel::Builder(std::make_unique<ui::DialogModelDelegate>())
          .SetTitle(u"Delete address?")
          .AddOkButton(
              base::BindOnce(&DeleteAddressProfileDialogController::OnAccepted,
                             controller),
              ui::DialogModelButton::Params().SetLabel(u"Delete"))
          .AddCancelButton(
              base::BindOnce(&DeleteAddressProfileDialogController::OnCanceled,
                             controller),
              ui::DialogModelButton::Params().SetLabel(u"Cancel"))
          .AddParagraph(
              ui::DialogModelLabel(
                  u"This address will be deleted from your Google account")
                  .set_is_secondary())
          .SetCloseActionCallback(base::BindOnce(
              &DeleteAddressProfileDialogController::OnAccepted, controller))
          .SetDialogDestroyingCallback(base::BindOnce(
              &DeleteAddressProfileDialogController::OnDialogDestroying,
              controller))
          .Build();
  return constrained_window::ShowWebModal(std::move(dialog_model),
                                          web_contents);
}

}  // namespace autofill::dialogs
