// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/model_predictions.h"

#include <memory>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

ModelPredictions::ModelPredictions(
    HeuristicSource source,
    FieldTypeSet supported_types,
    base::flat_map<FieldGlobalId, FieldType> predictions)
    : source_(source),
      supported_types_(std::move(supported_types)),
      predictions_(std::move(predictions)) {}

ModelPredictions::ModelPredictions(const ModelPredictions&) = default;

ModelPredictions::ModelPredictions(ModelPredictions&&) = default;

ModelPredictions& ModelPredictions::operator=(const ModelPredictions&) =
    default;

ModelPredictions& ModelPredictions::operator=(ModelPredictions&&) = default;

ModelPredictions::~ModelPredictions() = default;

// static
void ModelPredictions::Reset(
    HeuristicSource source,
    base::span<const std::unique_ptr<AutofillField>> fields) {
  ModelPredictions(source, {}, {}).ApplyTo(fields);
}

void ModelPredictions::ApplyTo(
    base::span<const std::unique_ptr<AutofillField>> fields) const {
  for (const std::unique_ptr<AutofillField>& field : fields) {
    auto it = predictions_.find(field->global_id());
    const FieldType type =
        it != predictions_.end() ? it->second : NO_SERVER_DATA;
    field->set_heuristic_type(source_, type);
    // `AutofillField` stores a single `ml_supported_types_` set that is only
    // read for `kAutofillMachineLearning` (see
    // `AutofillField::heuristic_type()`), so other ML sources must not
    // overwrite or clear it.
    if (source_ == HeuristicSource::kAutofillMachineLearning) {
      field->set_ml_supported_types(it != predictions_.end() ? supported_types_
                                                             : FieldTypeSet{});
    }
  }
}

}  // namespace autofill
