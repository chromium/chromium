// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_MODEL_EXECUTOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_MODEL_EXECUTOR_H_

#include <vector>

#include "base/files/file_path.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/ml_model/autofill_model_vectorizer.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/optimization_guide/core/base_model_executor.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill {

// Implements BaseModelExecutor to execute models with FormFieldData input and
// ServerFieldType output.
class AutofillModelExecutor
    : public optimization_guide::BaseModelExecutor<ServerFieldType,
                                                   const FormFieldData&> {
 public:
  AutofillModelExecutor();
  ~AutofillModelExecutor() override;

 protected:
  // Array describes how the output of the ML model is interpreted.
  // Indices 20 and 33 used to hold field types ADDRESS_HOME_SEARCH_WIDGET
  // and ADDITIONAL_PHONE_NUMBER respectively, but were changed to UNKNOWN_TYPE
  // since these two server field types are not supported on
  // client side.
  // TODO(crbug.com/1465926): Download dynamically from the server instead.
  static constexpr std::array<ServerFieldType, 34> kSupportedFieldTypes = {
      UNKNOWN_TYPE,
      CREDIT_CARD_VERIFICATION_CODE,
      CREDIT_CARD_NAME_FIRST,
      NAME_LAST,
      NAME_FULL,
      ADDRESS_HOME_LINE2,
      CREDIT_CARD_EXP_2_DIGIT_YEAR,
      CREDIT_CARD_NAME_LAST,
      ADDRESS_HOME_COUNTRY,
      CREDIT_CARD_EXP_4_DIGIT_YEAR,
      PHONE_HOME_CITY_AND_NUMBER,
      COMPANY_NAME,
      EMAIL_ADDRESS,
      ADDRESS_HOME_CITY,
      ADDRESS_HOME_LINE1,
      ADDRESS_HOME_STREET_ADDRESS,
      PHONE_HOME_EXTENSION,
      CREDIT_CARD_TYPE,
      NAME_HONORIFIC_PREFIX,
      DELIVERY_INSTRUCTIONS,
      UNKNOWN_TYPE,
      ADDRESS_HOME_LINE3,
      CREDIT_CARD_NUMBER,
      PHONE_HOME_COUNTRY_CODE,
      ADDRESS_HOME_STATE,
      CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
      ADDRESS_HOME_ZIP,
      PHONE_HOME_WHOLE_NUMBER,
      NAME_FIRST,
      CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
      CREDIT_CARD_NAME_FULL,
      CREDIT_CARD_EXP_MONTH,
      PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX,
      UNKNOWN_TYPE};

  // optimization_guide::BaseModelExecutor:
  // This function must be called on a background thread.
  // It initilaizes the vectorizer by reading the dictionary file
  // which can't be done on the UI thread.
  bool Preprocess(const std::vector<TfLiteTensor*>& input_tensors,
                  const FormFieldData& input) override;
  absl::optional<ServerFieldType> Postprocess(
      const std::vector<const TfLiteTensor*>& output_tensors) override;

  std::unique_ptr<AutofillModelVectorizer> vectorizer_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_MODEL_EXECUTOR_H_
