// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_parsing/fuzzer/form_predictions_producer.h"

#include <bitset>
#include <string>
#include <utility>

#include <fuzzer/FuzzedDataProvider.h>

#include "build/build_config.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/form_parsing/password_field_prediction.h"
#include "url/gurl.h"
#include "url/origin.h"

using autofill::FormData;
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

}  // namespace

FormPredictions GenerateFormPredictions(const FormData& form_data,
                                        FuzzedDataProvider& provider) {
  FormPredictions predictions;

  predictions.driver_id = provider.ConsumeIntegral<int>();
  predictions.form_signature =
      autofill::FormSignature(provider.ConsumeIntegral<uint64_t>());

  for (const auto& field : form_data.fields) {
    // Batch getting bits from the FuzzedDataProvider, because calling
    // `ConsumeBool` throws out 7 bits and we need multiple Booleans for each
    // iteration.
    const std::bitset<8> bools(provider.ConsumeIntegral<uint8_t>());
    const bool generate_prediction = bools[0];
    const bool pick_meaningful_type = bools[1];
    const bool use_placeholder = bools[2];

    if (generate_prediction) {
      PasswordFieldPrediction field_prediction;
      SetPredictionType(pick_meaningful_type, provider, field_prediction);
      field_prediction.may_use_prefilled_placeholder = use_placeholder;
      field_prediction.renderer_id = field.unique_renderer_id;
      predictions.fields.push_back(std::move(field_prediction));
    }
  }

  // Generate predictions for non-existing fields.
  const size_t num_predictions = provider.ConsumeIntegralInRange(0, 15);
  for (size_t i = 0; i < num_predictions; ++i) {
    const std::bitset<8> bools(provider.ConsumeIntegral<uint8_t>());
    const bool pick_meaningful_type = bools[0];
    const bool use_placeholder = bools[1];

    PasswordFieldPrediction field_prediction;
    SetPredictionType(pick_meaningful_type, provider, field_prediction);
    field_prediction.may_use_prefilled_placeholder = use_placeholder;
    field_prediction.renderer_id =
        autofill::FieldRendererId(provider.ConsumeIntegralInRange(-32, 31));
    predictions.fields.push_back(std::move(field_prediction));
  }

  return predictions;
}

}  // namespace password_manager
