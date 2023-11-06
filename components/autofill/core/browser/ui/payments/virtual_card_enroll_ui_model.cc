// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/virtual_card_enroll_ui_model.h"

#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

// static
VirtualCardEnrollUiModel VirtualCardEnrollUiModel::Create(
    const VirtualCardEnrollmentFields& enrollment_fields) {
  VirtualCardEnrollUiModel model;
  model.window_title = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_DIALOG_TITLE_LABEL);
  std::u16string learn_more_link_text = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_LEARN_MORE_LINK_LABEL);
  model.explanatory_message = l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_DIALOG_CONTENT_LABEL,
      learn_more_link_text);
  model.accept_action_text = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_ACCEPT_BUTTON_LABEL);
  model.cancel_action_text = l10n_util::GetStringUTF16(
      enrollment_fields.virtual_card_enrollment_source ==
              VirtualCardEnrollmentSource::kSettingsPage
          ? IDS_CANCEL
      : enrollment_fields.last_show
          ? IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_DECLINE_BUTTON_LABEL_NO_THANKS
          : IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_DECLINE_BUTTON_LABEL_SKIP);
  model.learn_more_link_text = learn_more_link_text;
  model.enrollment_fields = enrollment_fields;
  return model;
}

VirtualCardEnrollUiModel::VirtualCardEnrollUiModel() = default;
VirtualCardEnrollUiModel::~VirtualCardEnrollUiModel() = default;
bool VirtualCardEnrollUiModel::operator==(
    const VirtualCardEnrollUiModel& other) const = default;

VirtualCardEnrollUiModel::VirtualCardEnrollUiModel(
    const VirtualCardEnrollUiModel& other) = default;
VirtualCardEnrollUiModel& VirtualCardEnrollUiModel::operator=(
    const VirtualCardEnrollUiModel& other) noexcept = default;

VirtualCardEnrollUiModel::VirtualCardEnrollUiModel(
    VirtualCardEnrollUiModel&& other) = default;
VirtualCardEnrollUiModel& VirtualCardEnrollUiModel::operator=(
    VirtualCardEnrollUiModel&& other) noexcept = default;

}  // namespace autofill
