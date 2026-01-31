// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include <iostream>

#include "base/base64.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_encoding.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
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

std::string SerializeAndEncode(const AutofillQueryResponse& response) {
  std::string unencoded_response_string;
  if (!response.SerializeToString(&unencoded_response_string)) {
    LOG(ERROR) << "Cannot serialize the response proto";
    return "";
  }
  return base::Base64Encode(unencoded_response_string);
}

// We run ParseServerPredictionsFromQueryResponse twice with hardcoded forms
// vectors. Ideally we should also generate forms vectors by using fuzzing, but
// at the moment we use simplified approach. There is no specific reason to use
// those two hardcoded forms vectors, so it can be changed if needed.
DEFINE_BINARY_PROTO_FUZZER(const AutofillQueryResponse& response) {
  std::vector<FormData> forms;
  ParseServerPredictionsFromQueryResponse(
      SerializeAndEncode(response), forms, test::GetEncodedSignatures(forms),
      /*log_manager=*/nullptr, /*ignore_small_forms=*/true);

  FormData form;
  AddField("username", "username", FormControlType::kInputText, &form);
  AddField("password", "password", FormControlType::kInputPassword, &form);

  forms.emplace_back(form);
  ParseServerPredictionsFromQueryResponse(
      SerializeAndEncode(response), forms, test::GetEncodedSignatures(forms),
      /*log_manager=*/nullptr, /*ignore_small_forms=*/true);
}

}  // namespace
}  // namespace autofill
