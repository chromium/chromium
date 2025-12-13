// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_MODEL_PREDICTIONS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_MODEL_PREDICTIONS_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/is_required.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

class AutofillField;

// Holds the predictions returned by FieldClassificationModelHandler.
class ModelPredictions {
 public:
  ModelPredictions(HeuristicSource source,
                   FieldTypeSet supported_types,
                   base::flat_map<FieldGlobalId, FieldType> predictions);
  ModelPredictions(const ModelPredictions&);
  ModelPredictions(ModelPredictions&&);
  ModelPredictions& operator=(const ModelPredictions&);
  ModelPredictions& operator=(ModelPredictions&&);
  ~ModelPredictions();

  HeuristicSource source() const { return source_; }

  // Sets the heuristic types of `fields` according to `this`.
  void ApplyTo(base::span<const std::unique_ptr<AutofillField>> fields) const;

 private:
  HeuristicSource source_ = internal::IsRequired();
  FieldTypeSet supported_types_;
  base::flat_map<FieldGlobalId, FieldType> predictions_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_MODEL_PREDICTIONS_H_
