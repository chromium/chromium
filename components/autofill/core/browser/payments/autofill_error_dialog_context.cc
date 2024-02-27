// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"

namespace autofill {

// static
AutofillErrorDialogContext
AutofillErrorDialogContext::WithVirtualCardPermanentOrTemporaryError(
    bool is_permanent_error) {
  AutofillErrorDialogContext autofill_error_dialog_context;
  autofill_error_dialog_context.type =
      is_permanent_error ? AutofillErrorDialogType::kVirtualCardPermanentError
                         : AutofillErrorDialogType::kVirtualCardTemporaryError;
  return autofill_error_dialog_context;
}

AutofillErrorDialogContext::AutofillErrorDialogContext() = default;

AutofillErrorDialogContext::AutofillErrorDialogContext(
    const AutofillErrorDialogContext& other) = default;

AutofillErrorDialogContext::AutofillErrorDialogContext(
    AutofillErrorDialogContext&& other) = default;

AutofillErrorDialogContext& AutofillErrorDialogContext::operator=(
    const AutofillErrorDialogContext&) = default;

AutofillErrorDialogContext& AutofillErrorDialogContext::operator=(
    AutofillErrorDialogContext&&) = default;

AutofillErrorDialogContext::~AutofillErrorDialogContext() = default;

bool AutofillErrorDialogContext::operator==(
    const AutofillErrorDialogContext& other_context) const = default;

}  // namespace autofill
