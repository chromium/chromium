// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/actor/actor_critical_action_util.h"

#include <optional>

#include "base/notreached.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/critical_actions/core/browser/critical_action_types.h"

namespace autofill {

using critical_actions::FormFillType;

std::optional<FormFillType> ToFormFillType(FieldTypeGroup group) {
  switch (group) {
    case FieldTypeGroup::kAddress:
    case FieldTypeGroup::kCompany:
      return FormFillType::kAddress;
    case FieldTypeGroup::kName:
      return FormFillType::kName;
    case FieldTypeGroup::kEmail:
      return FormFillType::kEmail;
    case FieldTypeGroup::kPhone:
      return FormFillType::kPhone;
    case FieldTypeGroup::kCreditCard:
    case FieldTypeGroup::kStandaloneCvcField:
      return FormFillType::kCreditCard;
    // Not supported yet.
    case FieldTypeGroup::kIban:
    case FieldTypeGroup::kLoyaltyCard:
    case FieldTypeGroup::kAutofillAi:
      return std::nullopt;
    case FieldTypeGroup::kNoGroup:
    case FieldTypeGroup::kPasswordField:
    case FieldTypeGroup::kUsernameField:
    case FieldTypeGroup::kTransaction:
    case FieldTypeGroup::kUnfillable:
    case FieldTypeGroup::kOneTimePassword:
      return std::nullopt;
  }
  NOTREACHED();
}

}  // namespace autofill
