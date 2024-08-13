// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_parsing/fuzzer/form_predictions_producer.h"

#include <fuzzer/FuzzedDataProvider.h>

#include <algorithm>
#include <bitset>
#include <string>
#include <utility>

#include "build/build_config.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/form_parsing/password_field_prediction.h"
#include "url/gurl.h"
#include "url/origin.h"

using autofill::FieldType;
using autofill::FormData;

namespace password_manager {

namespace {

constexpr FieldType kPasswordRelatedServerTypes[] = {
    autofill::USERNAME,     autofill::USERNAME_AND_EMAIL_ADDRESS,
    autofill::PASSWORD,     autofill::ACCOUNT_CREATION_PASSWORD,
    autofill::NEW_PASSWORD, autofill::CONFIRMATION_PASSWORD,
    autofill::NOT_PASSWORD};

FieldType GeneratePredictionType(bool pick_meaningful_type,
                                 FuzzedDataProvider& provider) {
  if (pick_meaningful_type) {
    return provider.PickValueInArray(kPasswordRelatedServerTypes);
  } else {
    // Set a random type, probably even invalid.
    return static_cast<FieldType>(provider.ConsumeIntegral<uint8_t>());
  }
}

}  // namespace

FormPredictions GenerateFormPredictions(const FormData& form_data,
                                        FuzzedDataProvider& provider) {
  FormPredictions predictions;

  predictions.driver_id = provider.ConsumeIntegral<int>();
  predictions.form_signature =
      autofill::FormSignature(provider.ConsumeIntegral<uint64_t>());

  for (const auto& field : form_data.fields()) {
    // Batch getting bits from the FuzzedDataProvider, because calling
    // `ConsumeBool` throws out 7 bits and we need multiple Booleans for each
    // iteration.
    const std::bitset<8> bools(provider.ConsumeIntegral<uint8_t>());
    const bool generate_prediction = bools[0];
    const bool pick_meaningful_type = bools[1];
    const bool use_placeholder = bools[2];

    if (generate_prediction) {
      predictions.fields.emplace_back(
          field.renderer_id(), autofill::FieldSignature(123),
          GeneratePredictionType(pick_meaningful_type, provider),
          use_placeholder, /*is_override=*/false);
    }
  }

  // Generate predictions for non-existing fields.
  const size_t num_predictions = provider.ConsumeIntegralInRange(0, 15);
  for (size_t i = 0; i < num_predictions; ++i) {
    const std::bitset<8> bools(provider.ConsumeIntegral<uint8_t>());
    const bool pick_meaningful_type = bools[0];
    const bool use_placeholder = bools[1];

    // Generate unique `renderer_id` not matching any existing field
    // renderer_id.
    autofill::FieldRendererId renderer_id(
        provider.ConsumeIntegralInRange(-33, 31));
    while (std::any_of(form_data.fields().begin(), form_data.fields().end(),
                       [renderer_id](const autofill::FormFieldData& field) {
                         return renderer_id.value() ==
                                field.renderer_id().value();
                       })) {
      renderer_id =
          autofill::FieldRendererId(provider.ConsumeIntegralInRange(-33, 31));
    }

    autofill::FieldSignature signature(provider.ConsumeIntegralInRange(0, 500));
    predictions.fields.emplace_back(
        renderer_id, signature,
        GeneratePredictionType(pick_meaningful_type, provider), use_placeholder,
        /*is_override=*/false);
  }

  return predictions;
}

}  // namespace password_manager
