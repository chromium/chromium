// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/autofill_error_dialog_controller_impl.h"

#include "base/metrics/histogram_functions.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

AutofillErrorDialogControllerImpl::AutofillErrorDialogControllerImpl(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

AutofillErrorDialogControllerImpl::~AutofillErrorDialogControllerImpl() {
  Dismiss();
}

void AutofillErrorDialogControllerImpl::Show(
    const AutofillErrorDialogContext& autofill_error_dialog_context) {
  if (autofill_error_dialog_view_)
    Dismiss();

  DCHECK(autofill_error_dialog_view_ == nullptr);
  error_dialog_context_ = autofill_error_dialog_context;
  autofill_error_dialog_view_ = AutofillErrorDialogView::CreateAndShow(this);
  base::UmaHistogramEnumeration("Autofill.ErrorDialogShown",
                                autofill_error_dialog_context.type);
}

void AutofillErrorDialogControllerImpl::OnDismissed() {
  // TODO(crbug.com/1196021): Log the dismiss action along with the type of the
  // error dialog.
  autofill_error_dialog_view_ = nullptr;
}

const std::u16string AutofillErrorDialogControllerImpl::GetTitle() {
  int title_string_resource_id = 0;
  switch (error_dialog_context_.type) {
    case AutofillErrorDialogType::kVirtualCardTemporaryError:
      title_string_resource_id =
          IDS_AUTOFILL_VIRTUAL_CARD_TEMPORARY_ERROR_TITLE;
      break;
    case AutofillErrorDialogType::kVirtualCardPermanentError:
      title_string_resource_id =
          IDS_AUTOFILL_VIRTUAL_CARD_PERMANENT_ERROR_TITLE;
      break;
    case AutofillErrorDialogType::kVirtualCardNotEligibleError:
      title_string_resource_id =
          IDS_AUTOFILL_VIRTUAL_CARD_NOT_ELIGIBLE_ERROR_TITLE;
      break;
    case AutofillErrorDialogType::kTypeUnknown:
      NOTREACHED();
      break;
  }
  return title_string_resource_id != 0
             ? l10n_util::GetStringUTF16(title_string_resource_id)
             : std::u16string();
}

const std::u16string AutofillErrorDialogControllerImpl::GetDescription() {
  int description_string_resource_id = 0;
  switch (error_dialog_context_.type) {
    case AutofillErrorDialogType::kVirtualCardTemporaryError:
      description_string_resource_id =
          IDS_AUTOFILL_VIRTUAL_CARD_TEMPORARY_ERROR_DESCRIPTION;
      break;
    case AutofillErrorDialogType::kVirtualCardPermanentError:
      description_string_resource_id =
          IDS_AUTOFILL_VIRTUAL_CARD_PERMANENT_ERROR_DESCRIPTION;
      break;
    case AutofillErrorDialogType::kVirtualCardNotEligibleError:
      description_string_resource_id =
          IDS_AUTOFILL_VIRTUAL_CARD_NOT_ELIGIBLE_ERROR_DESCRIPTION;
      break;
    case AutofillErrorDialogType::kTypeUnknown:
      NOTREACHED();
      break;
  }
  return description_string_resource_id != 0
             ? l10n_util::GetStringUTF16(description_string_resource_id)
             : std::u16string();
}

const std::u16string AutofillErrorDialogControllerImpl::GetButtonLabel() {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_ERROR_DIALOG_NEGATIVE_BUTTON_LABEL);
}

content::WebContents* AutofillErrorDialogControllerImpl::GetWebContents() {
  return web_contents_;
}

void AutofillErrorDialogControllerImpl::Dismiss() {
  if (autofill_error_dialog_view_)
    autofill_error_dialog_view_->Dismiss();
}

}  // namespace autofill
