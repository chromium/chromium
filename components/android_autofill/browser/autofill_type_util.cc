// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/autofill_type_util.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/containers/flat_map.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_server_prediction.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/form_parsing/form_data_parser.h"
#include "components/password_manager/core/browser/password_form.h"

namespace autofill {

FieldType GetMostRelevantFieldType(const AutofillType& type) {
  if (FieldType field_type = type.GetPasswordManagerType();
      field_type != UNKNOWN_TYPE) {
    return field_type;
  }
  if (FieldType field_type = type.GetCreditCardType();
      field_type != UNKNOWN_TYPE) {
    return field_type;
  }
  if (FieldType field_type = type.GetAddressType();
      field_type != UNKNOWN_TYPE) {
    return field_type;
  }
  if (FieldTypeSet types = type.GetTypes(); !types.empty()) {
    return *types.begin();
  }
  return UNKNOWN_TYPE;
}

std::unique_ptr<password_manager::PasswordForm> ParseToPasswordForm(
    const FormStructure& form_structure) {
  // Transform the predictions data to a format the `FormDataParser` can handle
  // and parse the form.
  FormData form_data = form_structure.ToFormData();
  auto autofill_predictions =
      base::MakeFlatMap<FieldGlobalId, AutofillServerPrediction>(
          form_structure, /*comp=*/{},
          /*proj=*/[](const std::unique_ptr<AutofillField>& field) {
            return std::make_pair(field->global_id(),
                                  AutofillServerPrediction(*field));
          });
  password_manager::FormDataParser parser;
  parser.set_server_predictions(password_manager::ConvertToFormPredictions(
      form_data, autofill_predictions));
  // On Chrome, the parser can use stored usernames to identify a filled
  // username field by the value it contains. Since we do not have access to
  // credentials, we leave it empty.
  // UKM source id is required for recording metrics for parsing with the
  // clientside TFLite model, and TFLite is not available on WebView.
  return parser.Parse(form_data,
                      password_manager::FormDataParser::Mode::kFilling,
                      /*stored_usernames=*/{},
                      /*ukm_source_id=*/std::nullopt);
}

}  // namespace autofill
