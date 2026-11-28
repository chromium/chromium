// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_cache.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/map_util.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_format_string.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"

namespace autofill {

AutofillAiModelCache::FieldPrediction::FieldPrediction() = default;

AutofillAiModelCache::FieldPrediction::FieldPrediction(
    std::vector<FieldType> field_types,
    std::optional<AutofillFormatString> format_string)
    : field_types(std::move(field_types)),
      format_string(std::move(format_string)) {}

AutofillAiModelCache::FieldPrediction::FieldPrediction(const FieldPrediction&) =
    default;

AutofillAiModelCache::FieldPrediction&
AutofillAiModelCache::FieldPrediction::operator=(const FieldPrediction&) =
    default;

AutofillAiModelCache::FieldPrediction::FieldPrediction(FieldPrediction&&) =
    default;

AutofillAiModelCache::FieldPrediction&
AutofillAiModelCache::FieldPrediction::operator=(FieldPrediction&&) = default;

AutofillAiModelCache::FieldPrediction::~FieldPrediction() = default;

AutofillAiModelPredictions::AutofillAiModelPredictions(
    base::flat_map<AutofillAiModelCache::FieldIdentifier,
                   AutofillAiModelCache::FieldPrediction> predictions)
    : predictions_(std::move(predictions)) {}

AutofillAiModelPredictions::AutofillAiModelPredictions(
    const AutofillAiModelPredictions&) = default;

AutofillAiModelPredictions::AutofillAiModelPredictions(
    AutofillAiModelPredictions&&) noexcept = default;

AutofillAiModelPredictions& AutofillAiModelPredictions::operator=(
    const AutofillAiModelPredictions&) = default;

AutofillAiModelPredictions& AutofillAiModelPredictions::operator=(
    AutofillAiModelPredictions&&) noexcept = default;

AutofillAiModelPredictions::~AutofillAiModelPredictions() = default;

void AutofillAiModelPredictions::ApplyTo(FormStructure& form) const {
  for (const std::unique_ptr<AutofillField>& field : form.fields()) {
    const AutofillAiModelCache::FieldPrediction* prediction = base::FindOrNull(
        predictions_,
        AutofillAiModelCache::FieldIdentifier{
            .signature = field->GetFieldSignature(),
            .rank_in_signature_group = field->rank_in_signature_group()});
    if (!prediction) {
      continue;
    }
    for (FieldType type : prediction->field_types) {
      FieldPrediction server_prediction;
      server_prediction.set_type(type);
      server_prediction.set_source(FieldPrediction::SOURCE_AUTOFILL_AI);
      field->MaybeAddServerPrediction(std::move(server_prediction));
    }
    if (prediction->format_string &&
        AutofillFormatString::IsValid(prediction->format_string->value,
                                      prediction->format_string->type)) {
      field->set_format_string_unless_overruled(
          *prediction->format_string, AutofillFormatStringSource::kModelResult);
    }
  }
}

}  // namespace autofill
