// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_tapjacking_protector_impl.h"

#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

AutofillTapjackingProtector::AuthorizationResult ToAuthorizationResult(
    AutofillDialogController::Result result) {
  switch (result) {
    case AutofillDialogController::Result::kAccepted:
      return AutofillTapjackingProtector::AuthorizationResult::kSuccess;
    case AutofillDialogController::Result::kDeclined:
      return AutofillTapjackingProtector::AuthorizationResult::kCancelled;
    case AutofillDialogController::Result::kUnknown:
      return AutofillTapjackingProtector::AuthorizationResult::kUnknown;
  }
  NOTREACHED();
}

void OnDialogResult(AutofillTapjackingProtector::AuthorizationCallback callback,
                    AutofillDialogController::Result result) {
  std::move(callback).Run(ToAuthorizationResult(result));
}

}  // namespace

AutofillTapjackingProtectorImpl::AutofillTapjackingProtectorImpl(
    AutofillDialogController* autofill_dialog_controller)
    : autofill_dialog_controller_(autofill_dialog_controller) {}

AutofillTapjackingProtectorImpl::~AutofillTapjackingProtectorImpl() {
  // TODO(crbug.com/553136901): Dismiss any active dialogs shown by this
  // tapjacking protector.
}

void AutofillTapjackingProtectorImpl::Show(AuthorizationType authorization_type,
                                           AuthorizationCallback callback) {
  if (!autofill_dialog_controller_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), AuthorizationResult::kUnknown));
    return;
  }

  autofill_dialog_controller_->Show(
      GetDialogTitle(authorization_type),
      GetDialogDescription(authorization_type),
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_TAPJACKING_CONFIRMATION_DIALOG_POSITIVE_BUTTON),
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_TAPJACKING_CONFIRMATION_DIALOG_NEGATIVE_BUTTON),
      base::BindOnce(&OnDialogResult, std::move(callback)));
}

std::u16string AutofillTapjackingProtectorImpl::GetDialogTitle(
    AuthorizationType authorization_type) const {
  switch (authorization_type) {
    case AuthorizationType::kPayments:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_TAPJACKING_CREDIT_CARD_CONFIRMATION_DIALOG_TITLE);
  }
  NOTREACHED();
}

std::u16string AutofillTapjackingProtectorImpl::GetDialogDescription(
    AuthorizationType authorization_type) const {
  switch (authorization_type) {
    case AuthorizationType::kPayments:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_TAPJACKING_CREDIT_CARD_CONFIRMATION_DIALOG_DESCRIPTION);
  }
  NOTREACHED();
}

}  // namespace autofill
