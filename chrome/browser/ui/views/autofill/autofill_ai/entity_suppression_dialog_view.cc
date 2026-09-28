// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_ai/entity_suppression_dialog_view.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/dialogs/browser_dialogs.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/window/dialog_client_view.h"

namespace autofill {

void ShowEntitySuppressionDialogView(content::WebContents* web_contents,
                                     int num_sources,
                                     base::OnceCallback<void(bool)> callback) {
  auto [on_accept, on_dismiss] = base::SplitOnceCallback(std::move(callback));
  auto [on_cancel, on_close] = base::SplitOnceCallback(std::move(on_dismiss));

  auto dialog_model =
      ui::DialogModel::Builder(std::make_unique<ui::DialogModelDelegate>())
          .SetInternalName(kEntitySuppressionDialogName)
          .SetTitle(l10n_util::GetStringUTF16(
              IDS_AUTOFILL_AI_ENTITY_SUPPRESSION_CONFIRMATION_TITLE))
          .AddParagraph(
              ui::DialogModelLabel(
                  l10n_util::GetPluralStringFUTF16(
                      IDS_AUTOFILL_AI_ENTITY_SUPPRESSION_CONFIRMATION_BODY,
                      num_sources))
                  .set_is_secondary())
          .AddOkButton(
              base::BindOnce(std::move(on_accept), true),
              ui::DialogModel::Button::Params()
                  .SetId(views::DialogClientView::kOkButtonElementId)
                  .SetLabel(l10n_util::GetStringUTF16(
                      IDS_AUTOFILL_AI_ENTITY_SUPPRESSION_CONFIRMATION_ACCEPT_BUTTON)))
          .AddCancelButton(
              base::BindOnce(std::move(on_cancel), false),
              ui::DialogModel::Button::Params()
                  .SetId(views::DialogClientView::kCancelButtonElementId)
                  .SetLabel(l10n_util::GetStringUTF16(IDS_CANCEL)))
          .SetCloseActionCallback(base::BindOnce(std::move(on_close), false))
          .Build();

  chrome::ShowTabModal(std::move(dialog_model), web_contents);
}

}  // namespace autofill
