// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include <iostream>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_encoding.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

namespace autofill {
namespace {

using ::base::ASCIIToUTF16;

void AddField(const std::string& label,
              const std::string& name,
              FormControlType control_type,
              FormData* form_data) {
  FormFieldData field;
  field.set_label(ASCIIToUTF16(label));
  field.set_name(ASCIIToUTF16(name));
  field.set_form_control_type(control_type);
  test_api(*form_data).Append(field);
}

// We run ProcessServerPredictionsQueryResponse twice with hardcoded forms
// vectors. Ideally we should also generate forms vectors by using fuzzing, but
// at the moment we use simplified approach. There is no specific reason to use
// those two hardcoded forms vectors, so it can be changed if needed.
DEFINE_BINARY_PROTO_FUZZER(const AutofillQueryResponse& response) {
  std::vector<raw_ptr<FormStructure, VectorExperimental>> forms;
  ProcessServerPredictionsQueryResponse(
      response, forms, test::GetEncodedSignatures(forms),
      /*form_interactions_ukm_logger=*/nullptr,
      /*log_manager=*/nullptr);

  FormData form_data;
  AddField("username", "username", FormControlType::kInputText, &form_data);
  AddField("password", "password", FormControlType::kInputPassword, &form_data);

  FormStructure form(form_data);
  forms.push_back(&form);
  ProcessServerPredictionsQueryResponse(
      response, forms, test::GetEncodedSignatures(forms),
      /*form_interactions_ukm_logger=*/nullptr,
      /*log_manager=*/nullptr);
}

}  // namespace
}  // namespace autofill
