// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/model_predictions.h"

#include "base/containers/flat_map.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/common/dense_set.h"
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

void ModelPredictions::ApplyTo(
    base::span<const std::unique_ptr<AutofillField>> fields) const {
  for (const std::unique_ptr<AutofillField>& field : fields) {
    auto it = predictions_.find(field->global_id());
    if (it == predictions_.end()) {
      continue;
    }
    field->set_heuristic_type(source_, it->second);
    field->set_ml_supported_types(supported_types_);
  }
}

}  // namespace autofill
