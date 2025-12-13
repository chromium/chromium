// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_suggestion.h"

#include <vector>

#include "base/containers/contains.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"

namespace autofill {

OtpFillData CreateFillDataForOtpSuggestion(const FormStructure& form,
                                           const AutofillField& trigger_field,
                                           const std::u16string& otp_value) {
  // Extract a list of all OTP fields in the form, sorted in form order.
  std::vector<FieldGlobalId> otp_field_ids;
  for (const auto& field : form.fields()) {
    if (field->Type().GetTypes().contains(ONE_TIME_CODE)) {
      otp_field_ids.push_back(field->global_id());
    }
  }

  const FieldGlobalId& field_id = trigger_field.global_id();

  // Check if the triggering field is not classified as OTP field anymore.
  if (!base::Contains(otp_field_ids, field_id)) {
    // The only way this could happen is if the form has changed between the
    // field focus and the filling moment. Fill into the current field, since
    // the user requested that and otherwise it would be weird.
    return {{field_id, otp_value}};
  }

  // If `otp_value` has more characters/digits than the number of OTP fields in
  // this form, we assume that the value is not split between fields, but is to
  // be filled in the triggering field.
  if (otp_value.length() > otp_field_ids.size()) {
    return {{field_id, otp_value}};
  }

  // If the length of `otp_value` matches the number of fields, split the value
  // char-by-char between all fields.
  if (otp_value.length() == otp_field_ids.size()) {
    OtpFillData fill_data;
    for (size_t i = 0; i < otp_field_ids.size(); ++i) {
      FieldGlobalId target_field_id = otp_field_ids[i];
      fill_data[target_field_id] = otp_value.substr(i, 1);
    }
    return fill_data;
  }

  // When the code flow reaches this line, the length of `otp_value` is smaller
  // than the number of OTP fields.

  // If filling character-by-character, starting from `field_id`, allows filling
  // the entire OTP value, fill it this way.
  const size_t start_index = std::distance(
      otp_field_ids.begin(), std::ranges::find(otp_field_ids, field_id));
  if (start_index + otp_value.length() <= otp_field_ids.size()) {
    OtpFillData fill_data;
    for (size_t i = 0; i < otp_value.length(); ++i) {
      FieldGlobalId target_field_id = otp_field_ids[start_index + i];
      fill_data[target_field_id] = otp_value.substr(i, 1);
    }
    return fill_data;
  }

  // All other cases are non-trivial, attempt to fill the value into the
  // triggering field as the best effort.
  return {{field_id, otp_value}};
}

}  // namespace autofill
