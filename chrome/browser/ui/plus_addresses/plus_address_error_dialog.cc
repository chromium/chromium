// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/plus_addresses/plus_address_error_dialog.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/plus_addresses/grit/plus_addresses_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"

namespace plus_addresses {

namespace {

std::unique_ptr<ui::DialogModel> CreateErrorDialogWithWithCancelAndAccept(
    std::u16string description,
    base::OnceClosure on_accepted) {
  return ui::DialogModel::Builder()
      .SetTitle(
          l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_CREATE_INLINE_ERROR_TITLE))
      .AddParagraph(ui::DialogModelLabel(std::move(description)))
      .AddCancelButton(base::DoNothing(),
                       ui::DialogModel::Button::Params().SetId(
                           kPlusAddressErrorDialogCancelButton))
      .AddOkButton(std::move(on_accepted),
                   ui::DialogModel::Button::Params()
                       .SetId(kPlusAddressErrorDialogAcceptButton)
                       .SetLabel(l10n_util::GetStringUTF16(
                           IDS_PLUS_ADDRESS_CREATE_INLINE_ERROR_ACCEPT_BUTTON)))
      .Build();
}

std::unique_ptr<ui::DialogModel> CreateGenericErrorDialog(
    base::OnceClosure on_accepted) {
  return CreateErrorDialogWithWithCancelAndAccept(
      /*description=*/l10n_util::GetStringUTF16(
          IDS_PLUS_ADDRESS_CREATE_INLINE_ERROR_DESCRIPTION),
      std::move(on_accepted));
}

std::unique_ptr<ui::DialogModel> CreateTimeoutErrorDialog(
    base::OnceClosure on_accepted) {
  return CreateErrorDialogWithWithCancelAndAccept(
      /*description=*/l10n_util::GetStringUTF16(
          IDS_PLUS_ADDRESS_CREATE_INLINE_TIMEOUT_ERROR_DESCRIPTION),
      std::move(on_accepted));
}

std::unique_ptr<ui::DialogModel> CreateQuotaErrorDialog(
    base::OnceClosure on_accepted) {
  return ui::DialogModel::Builder()
      .SetTitle(
          l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_CREATE_INLINE_ERROR_TITLE))
      .AddParagraph(ui::DialogModelLabel(l10n_util::GetStringUTF16(
          IDS_PLUS_ADDRESS_CREATE_INLINE_ERROR_DESCRIPTION)))
      .AddOkButton(std::move(on_accepted),
                   ui::DialogModel::Button::Params().SetId(
                       kPlusAddressErrorDialogAcceptButton))
      .Build();
}

}  // namespace

DEFINE_ELEMENT_IDENTIFIER_VALUE(kPlusAddressErrorDialogAcceptButton);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kPlusAddressErrorDialogCancelButton);

void ShowInlineCreationAffiliationErrorDialog(
    content::WebContents* web_contents,
    std::u16string affiliated_domain,
    std::u16string affiliated_plus_address,
    base::OnceClosure on_accepted) {
  constrained_window::ShowWebModal(
      ui::DialogModel::Builder()
          .SetTitle(l10n_util::GetStringUTF16(
              IDS_PLUS_ADDRESS_CREATE_INLINE_AFFILATION_ERROR_TITLE))
          .AddParagraph(ui::DialogModelLabel::CreateWithReplacements(
              IDS_PLUS_ADDRESS_CREATE_INLINE_AFFILATION_ERROR_DESCRIPTION,
              {ui::DialogModelLabel::CreateEmphasizedText(
                   std::move(affiliated_domain)),
               ui::DialogModelLabel::CreateEmphasizedText(
                   std::move(affiliated_plus_address))}))
          .AddCancelButton(base::DoNothing(),
                           ui::DialogModel::Button::Params().SetId(
                               kPlusAddressErrorDialogCancelButton))
          .AddOkButton(
              std::move(on_accepted),
              ui::DialogModel::Button::Params()
                  .SetId(kPlusAddressErrorDialogAcceptButton)
                  .SetLabel(l10n_util::GetStringUTF16(
                      IDS_PLUS_ADDRESS_CREATE_INLINE_AFFILATION_ERROR_ACCEPT_BUTTON)))
          .Build(),
      web_contents);
}

void ShowInlineCreationErrorDialog(content::WebContents* web_contents,
                                   PlusAddressErrorDialogType error_dialog_type,
                                   base::OnceClosure on_accepted) {
  auto create_model = [&]() {
    switch (error_dialog_type) {
      case PlusAddressErrorDialogType::kGenericError:
        return CreateGenericErrorDialog(std::move(on_accepted));
      case PlusAddressErrorDialogType::kTimeout:
        return CreateTimeoutErrorDialog(std::move(on_accepted));
      case PlusAddressErrorDialogType::kQuotaExhausted:
        return CreateQuotaErrorDialog(std::move(on_accepted));
    }
    NOTREACHED();
  };
  constrained_window::ShowWebModal(create_model(), web_contents);
}

}  // namespace plus_addresses
