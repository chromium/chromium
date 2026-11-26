// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_MODEL_PREDICTIONS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_MODEL_PREDICTIONS_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/heuristic_source.h"
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

  // Resets the heuristic types to `NO_SERVER_DATA` and clears ML metadata of
  // `fields` for `source`.
  static void Reset(HeuristicSource source,
                    base::span<const std::unique_ptr<AutofillField>> fields);

  // Sets (or resets to `NO_SERVER_DATA`) the heuristic types and ML metadata
  // of `fields` for `source_`.
  void ApplyTo(base::span<const std::unique_ptr<AutofillField>> fields) const;

 private:
  HeuristicSource source_ = internal::IsRequired();
  FieldTypeSet supported_types_;
  base::flat_map<FieldGlobalId, FieldType> predictions_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_MODEL_PREDICTIONS_H_
