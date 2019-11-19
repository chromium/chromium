// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_parsing/fuzzer/form_data_producer.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/password_manager/core/browser/form_parsing/fuzzer/data_accessor.h"
#include "url/gurl.h"
#include "url/origin.h"

using autofill::FormData;
using autofill::FormFieldData;
using autofill::ServerFieldType;

namespace password_manager {

namespace {

constexpr ServerFieldType kPasswordRelatedServerTypes[] = {
    autofill::USERNAME,     autofill::USERNAME_AND_EMAIL_ADDRESS,
    autofill::PASSWORD,     autofill::ACCOUNT_CREATION_PASSWORD,
    autofill::NEW_PASSWORD, autofill::CONFIRMATION_PASSWORD,
    autofill::NOT_PASSWORD};

struct FormFieldDataParams {
  size_t form_control_type_length;
  size_t autocomplete_attribute_length;
  size_t label_length;
  size_t name_length;
  size_t id_length;
  size_t value_length;
  // In an array of FormFieldData, all instances with |same_value_field| true
  // get the same value as the first such instance.
  bool same_value_field;
};

// With probability 0.5 returns true and generates |prediction|, with
// probability 0.5 returns false. Uses |accessor| as a source of randomness.
bool MaybeGenerateFieldPrediction(DataAccessor* accessor,
                                  PasswordFieldPrediction* prediction) {
  bool generate_prediction = accessor->ConsumeBit();
  if (!generate_prediction)
    return false;
  prediction->may_use_prefilled_placeholder = accessor->ConsumeBit();
  const size_t prediction_idx = accessor->ConsumeNumber(3);
  if (prediction_idx < base::size(kPasswordRelatedServerTypes)) {
    prediction->type = kPasswordRelatedServerTypes[prediction_idx];
  } else {
    // Set random type, probably even invalid. FormParser should gracefully
    // treat any type.
    prediction->type = static_cast<ServerFieldType>(accessor->ConsumeNumber(8));
  }
  return true;
}

}  // namespace

autofill::FormData GenerateWithDataAccessor(
    password_manager::DataAccessor* accessor,
    FormPredictions* predictions) {
  FormData result;

  // First determine the main non-string attributes not specific to particular
  // fields.
  result.is_form_tag = accessor->ConsumeBit();
  result.is_formless_checkout = accessor->ConsumeBit();

  // To minimize wasting bits, string-based data itself gets extracted after all
  // numbers and flags are. Their length can be determined now, however. A
  // reasonable range is 0-127 characters, i.e., 7 bits.
  const size_t name_length = accessor->ConsumeNumber(7);
  const size_t action_length = accessor->ConsumeNumber(7);
  const size_t origin_length = accessor->ConsumeNumber(7);
  const size_t main_frame_origin_length = accessor->ConsumeNumber(7);

  // Determine how many fields this form will have. 0-15, i.e., 4 bits.
  const size_t number_of_fields = accessor->ConsumeNumber(4);
  result.fields.resize(number_of_fields);
  FormFieldDataParams field_params[15];

  int first_field_with_same_value = -1;
  for (size_t i = 0; i < number_of_fields; ++i) {
    // Determine the non-string value for each field.
    result.fields[i].is_focusable = accessor->ConsumeBit();
    // And the lengths of the string values.
    field_params[i].form_control_type_length = accessor->ConsumeNumber(7);
    field_params[i].autocomplete_attribute_length = accessor->ConsumeNumber(7);
    field_params[i].label_length = accessor->ConsumeNumber(7);
    field_params[i].name_length = accessor->ConsumeNumber(7);
    field_params[i].id_length = accessor->ConsumeNumber(7);
    field_params[i].same_value_field = accessor->ConsumeBit();
    bool has_value_copy_from_earlier = field_params[i].same_value_field;
    if (field_params[i].same_value_field && first_field_with_same_value == -1) {
      first_field_with_same_value = static_cast<int>(i);
      has_value_copy_from_earlier = false;
    }
    // Emtpy values are interesting from the parsing perspective. Ensure that a
    // big chunk of the cases ends up with an empty value by letting an input
    // bit decide.
    field_params[i].value_length = 0;
    if (!has_value_copy_from_earlier && accessor->ConsumeBit()) {
      field_params[i].value_length = accessor->ConsumeNumber(7) + 1;
    }
  }

  // Now go back and determine the string-based values of the form itself.
  result.name = accessor->ConsumeString16(name_length);
  result.action = GURL(accessor->ConsumeString(action_length));
  result.url = GURL(accessor->ConsumeString(origin_length));
  result.main_frame_origin = url::Origin::Create(
      GURL(accessor->ConsumeString(main_frame_origin_length)));

  if (predictions) {
    predictions->driver_id = static_cast<int>(accessor->ConsumeNumber(32));
    predictions->form_signature =
        (static_cast<uint64_t>(accessor->ConsumeNumber(32)) << 32) +
        accessor->ConsumeNumber(32);
  }

  // And finally do the same for all the fields.
  for (size_t i = 0; i < number_of_fields; ++i) {
    result.fields[i].form_control_type =
        accessor->ConsumeString(field_params[i].form_control_type_length);
    result.fields[i].autocomplete_attribute =
        accessor->ConsumeString(field_params[i].autocomplete_attribute_length);
    result.fields[i].label =
        accessor->ConsumeString16(field_params[i].label_length);
    result.fields[i].name =
        accessor->ConsumeString16(field_params[i].name_length);
    result.fields[i].name_attribute = result.fields[i].name;
    result.fields[i].id_attribute =
        accessor->ConsumeString16(field_params[i].id_length);
    // Check both positive and negavites numbers for renderer ids.
    result.fields[i].unique_renderer_id =
        static_cast<uint32_t>(accessor->ConsumeNumber(6) - 32);
    if (predictions) {
      PasswordFieldPrediction field_prediction;
      if (MaybeGenerateFieldPrediction(accessor, &field_prediction)) {
        field_prediction.renderer_id = result.fields[i].unique_renderer_id;
        predictions->fields.push_back(field_prediction);
      }
    }

#if defined(OS_IOS)
    result.fields[i].unique_id = result.fields[i].id_attribute +
                                 base::UTF8ToUTF16("-") +
                                 base::NumberToString16(i);
#endif
    if (field_params[i].same_value_field &&
        first_field_with_same_value != static_cast<int>(i)) {
      result.fields[i].value = result.fields[first_field_with_same_value].value;
    } else {
      result.fields[i].value =
          accessor->ConsumeString16(field_params[i].value_length);
    }
  }

  if (predictions) {
    // Generate predictions for non-existing fields.
    size_t n_predictions = accessor->ConsumeNumber(4);
    PasswordFieldPrediction field_prediction;
    for (size_t i = 0; i < n_predictions; ++i) {
      if (MaybeGenerateFieldPrediction(accessor, &field_prediction)) {
        // Check both positive and negavites numbers for renderer ids.
        field_prediction.renderer_id =
            static_cast<uint32_t>(accessor->ConsumeNumber(6) - 32);
        predictions->fields.push_back(field_prediction);
      }
    }
  }

  return result;
}

}  // namespace password_manager
