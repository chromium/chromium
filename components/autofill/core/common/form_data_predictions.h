// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_FORM_DATA_PREDICTIONS_H__
#define COMPONENTS_AUTOFILL_CORE_COMMON_FORM_DATA_PREDICTIONS_H__

#include <string>
#include <vector>

#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data_predictions.h"

namespace autofill {

// Holds information about a form to be filled and/or submitted.
struct FormDataPredictions {
  FormData data;
  std::string signature;
  std::string alternative_signature;
  std::vector<FormFieldDataPredictions> fields;

  FormDataPredictions();
  FormDataPredictions(const FormDataPredictions&);
  FormDataPredictions& operator=(const FormDataPredictions&);
  FormDataPredictions(FormDataPredictions&&);
  FormDataPredictions& operator=(FormDataPredictions&&);
  ~FormDataPredictions();
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_FORM_DATA_PREDICTIONS_H__
