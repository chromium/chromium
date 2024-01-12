// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_FORM_FIELD_DATA_PREDICTIONS_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_FORM_FIELD_DATA_PREDICTIONS_H_

#include <optional>
#include <string>

#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

// Stores information about a field in a form.
struct FormFieldDataPredictions {
  FormFieldDataPredictions();
  FormFieldDataPredictions(const FormFieldDataPredictions&);
  FormFieldDataPredictions& operator=(const FormFieldDataPredictions&);
  FormFieldDataPredictions(FormFieldDataPredictions&&);
  FormFieldDataPredictions& operator=(FormFieldDataPredictions&&);
  ~FormFieldDataPredictions();

  friend bool operator==(const FormFieldDataPredictions&,
                         const FormFieldDataPredictions&);

  std::string host_form_signature;
  std::string signature;
  std::string heuristic_type;
  // std::nullopt if the server response has not arrived yet.
  std::optional<std::string> server_type;
  std::string html_type;
  std::string overall_type;
  std::string parseable_name;
  std::string section;
  size_t rank = 0;
  size_t rank_in_signature_group = 0;
  size_t rank_in_host_form = 0;
  size_t rank_in_host_form_signature_group = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_FORM_FIELD_DATA_PREDICTIONS_H_
