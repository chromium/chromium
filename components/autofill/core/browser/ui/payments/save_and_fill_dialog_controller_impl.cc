// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/save_and_fill_dialog_controller_impl.h"

#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

SaveAndFillDialogControllerImpl::SaveAndFillDialogControllerImpl() = default;
SaveAndFillDialogControllerImpl::~SaveAndFillDialogControllerImpl() = default;

void SaveAndFillDialogControllerImpl::ShowDialog(
    base::OnceCallback<std::unique_ptr<SaveAndFillDialogView>()>
        create_and_show_view_callback) {
  dialog_view_ = std::move(create_and_show_view_callback).Run();
  CHECK(dialog_view_);
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
std::u16string SaveAndFillDialogControllerImpl::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_TITLE);
}

std::u16string SaveAndFillDialogControllerImpl::GetExplanatoryMessage() const {
  return l10n_util::GetStringUTF16(
      IsUploadSaveAndFill()
          ? IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_EXPLANATION_UPLOAD
          : IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_EXPLANATION_LOCAL);
}

std::u16string SaveAndFillDialogControllerImpl::GetCardNumberLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_CARD_NUMBER_LABEL);
}

std::u16string SaveAndFillDialogControllerImpl::GetNameOnCardLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_NAME_ON_CARD_LABEL);
}

std::u16string SaveAndFillDialogControllerImpl::GetAcceptButtonText() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_ACCEPT);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

bool SaveAndFillDialogControllerImpl::IsUploadSaveAndFill() const {
  return is_upload_save_and_fill_;
}

base::WeakPtr<SaveAndFillDialogController>
SaveAndFillDialogControllerImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace autofill
