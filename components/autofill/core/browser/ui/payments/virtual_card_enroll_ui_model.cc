// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/virtual_card_enroll_ui_model.h"

#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

VirtualCardEnrollUiModel::Observer::~Observer() = default;

VirtualCardEnrollUiModel::VirtualCardEnrollUiModel(
    const VirtualCardEnrollmentFields& enrollment_fields)
    : window_title_(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_DIALOG_TITLE_LABEL)),
      accept_action_text_(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_ACCEPT_BUTTON_LABEL)),
      cancel_action_text_(l10n_util::GetStringUTF16(
          enrollment_fields.virtual_card_enrollment_source ==
                  VirtualCardEnrollmentSource::kSettingsPage
              ? IDS_CANCEL
          : enrollment_fields.last_show
              ? IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_DECLINE_BUTTON_LABEL_NO_THANKS
              : IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_DECLINE_BUTTON_LABEL_SKIP)),
      learn_more_link_text_(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_LEARN_MORE_LINK_LABEL)),
      enrollment_fields_(enrollment_fields) {
  explanatory_message_ = l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_DIALOG_CONTENT_LABEL,
      learn_more_link_text_);
}

VirtualCardEnrollUiModel::~VirtualCardEnrollUiModel() = default;

}  // namespace autofill
