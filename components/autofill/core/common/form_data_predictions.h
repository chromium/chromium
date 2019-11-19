// Copyright 2013 The Chromium Authors. All rights reserved.
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
  // Data for this form.
  FormData data;
  // The form signature for communication with the crowdsourcing server.
  std::string signature;
  // The form fields and their predicted field types.
  std::vector<FormFieldDataPredictions> fields;

  FormDataPredictions();
  FormDataPredictions(const FormDataPredictions&);
  FormDataPredictions& operator=(const FormDataPredictions&);
  FormDataPredictions(FormDataPredictions&&);
  FormDataPredictions& operator=(FormDataPredictions&&);
  ~FormDataPredictions();

  // Added for the sake of testing.
  bool operator==(const FormDataPredictions& predictions) const;
  bool operator!=(const FormDataPredictions& predictions) const;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_FORM_DATA_PREDICTIONS_H__
