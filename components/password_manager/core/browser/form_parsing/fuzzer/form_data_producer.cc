// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_parsing/fuzzer/form_data_producer.h"

#include <string>
#include <utility>

#include <fuzzer/FuzzedDataProvider.h>

#include "build/build_config.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/form_parsing/password_field_prediction.h"
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

void SetPredictionType(bool pick_meaningful_type,
                       FuzzedDataProvider& provider,
                       PasswordFieldPrediction& prediction) {
  if (pick_meaningful_type) {
    prediction.type = provider.PickValueInArray(kPasswordRelatedServerTypes);
  } else {
    // Set a random type, probably even invalid.
    prediction.type =
        static_cast<ServerFieldType>(provider.ConsumeIntegral<uint8_t>());
  }
}

// A wrapper to get |std::u16string| from a |FuzzedDataProvider|. The result is
// arbitrary bytes, not necessarily valid UTF16.
std::u16string ConsumeU16String(FuzzedDataProvider& provider) {
  // |FuzzedDataProvider| takes special steps to pick the length of the string
  // to support evolution of the fuzzed input. Let's follow whatever it does.
  const std::string s8 = provider.ConsumeRandomLengthString();
  return std::u16string(
      reinterpret_cast<const std::u16string::value_type*>(s8.data()),
      s8.size() / 2);
}

}  // namespace

autofill::FormData GenerateFormData(FuzzedDataProvider& provider,
                                    FormPredictions* predictions) {
  FormData result;

  result.is_form_tag = provider.ConsumeBool();

  // Determine how many fields this form will have. Pick a low value because
  // after the fuzzer's seed is exhausted, all will be 0s anyway.
  const size_t number_of_fields =
      provider.ConsumeIntegralInRange<size_t>(0, 15);
  result.fields.resize(number_of_fields);

  result.name = ConsumeU16String(provider);
  result.action = GURL(provider.ConsumeRandomLengthString());
  result.url = GURL(provider.ConsumeRandomLengthString());
  result.main_frame_origin =
      url::Origin::Create(GURL(provider.ConsumeRandomLengthString()));

  if (predictions) {
    predictions->driver_id = provider.ConsumeIntegral<int>();
    predictions->form_signature =
        autofill::FormSignature(provider.ConsumeIntegral<uint64_t>());
  }

  int first_field_with_same_value = -1;
  for (size_t i = 0; i < number_of_fields; ++i) {
    // Batch getting bits from the FuzzedDataProvider, because calling
    // `ConsumeBool` throws out 7 bits and we need many Booleans for each
    // iteration.
    const auto packed_bools = provider.ConsumeIntegral<uint8_t>();
    // All instances with |same_value_field| true share the same value.
    const bool same_value_field = packed_bools & 1;
    // Empty values are interesting from the parsing perspective. Ensure that
    // at least half of the cases ends up with an empty value.
    const bool force_empty_value = packed_bools & (1 << 1);
    result.fields[i].is_focusable = packed_bools & (1 << 2);
    const bool generate_prediction = packed_bools & (1 << 3);
    const bool pick_meaningful_type = packed_bools & (1 << 4);
    const bool use_placeholder = packed_bools & (1 << 5);

    result.fields[i].form_control_type =
        provider.ConsumeEnum<autofill::FormControlType>();
    result.fields[i].autocomplete_attribute =
        provider.ConsumeRandomLengthString();
    result.fields[i].label = ConsumeU16String(provider);
    result.fields[i].name = ConsumeU16String(provider);
    result.fields[i].name_attribute = result.fields[i].name;
    result.fields[i].id_attribute = ConsumeU16String(provider);
    result.fields[i].unique_renderer_id =
        autofill::FieldRendererId(provider.ConsumeIntegralInRange(-32, 31));
    if (predictions && generate_prediction) {
      PasswordFieldPrediction field_prediction;
      SetPredictionType(pick_meaningful_type, provider, field_prediction);
      field_prediction.may_use_prefilled_placeholder = use_placeholder;
      field_prediction.renderer_id = result.fields[i].unique_renderer_id;
      predictions->fields.push_back(std::move(field_prediction));
    }

    if (same_value_field) {
      if (first_field_with_same_value == -1) {
        first_field_with_same_value = static_cast<int>(i);
      } else {
        result.fields[i].value =
            result.fields[first_field_with_same_value].value;
      }
    } else if (!force_empty_value) {
      result.fields[i].value = ConsumeU16String(provider);
    }
  }

  if (!predictions) {
    return result;
  }

  // Generate predictions for non-existing fields.
  const size_t num_predictions = provider.ConsumeIntegralInRange(0, 15);
  for (size_t i = 0; i < num_predictions; ++i) {
    const auto packed_bools = provider.ConsumeIntegral<uint8_t>();
    const bool pick_meaningful_type = packed_bools & 1;
    const bool use_placeholder = packed_bools & (1 << 2);

    PasswordFieldPrediction field_prediction;
    SetPredictionType(pick_meaningful_type, provider, field_prediction);
    field_prediction.may_use_prefilled_placeholder = use_placeholder;
    field_prediction.renderer_id =
        autofill::FieldRendererId(provider.ConsumeIntegralInRange(-32, 31));
    predictions->fields.push_back(std::move(field_prediction));
  }

  return result;
}

}  // namespace password_manager
