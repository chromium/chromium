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

// Implements BaseModelExecutor to execute models with FormData input and
// std::vector<ServerFieldType> output. The executor only supports at most
// `kMaxNumberOfFields`. When calling the executor with a larger form,
// predictions are only returned for the first `kMaxNumberOfFields` many fields.
class AutofillModelExecutor
    : public optimization_guide::BaseModelExecutor<std::vector<ServerFieldType>,
                                                   const FormData&> {
 public:
  AutofillModelExecutor();
  ~AutofillModelExecutor() override;

 protected:
  // Array describes how the output of the ML model is interpreted.
  // Some of the types that the model was trained on are not supported by the
  // client. Index 0 is UNKNOWN_TYPE, while the others are non-supported types.
  // TODO(crbug.com/1465926): Download dynamically from the server instead.
  static constexpr std::array<ServerFieldType, 57> kSupportedFieldTypes = {
      UNKNOWN_TYPE,
      EMAIL_ADDRESS,
      UNKNOWN_TYPE,
      UNKNOWN_TYPE,
      UNKNOWN_TYPE,
      UNKNOWN_TYPE,
      CREDIT_CARD_NUMBER,
      CONFIRMATION_PASSWORD,
      UNKNOWN_TYPE,
      PHONE_HOME_EXTENSION,
      PHONE_HOME_WHOLE_NUMBER,
      PHONE_HOME_COUNTRY_CODE,
      UNKNOWN_TYPE,
      NAME_FIRST,
      ADDRESS_HOME_DEPENDENT_LOCALITY,
      ADDRESS_HOME_CITY,
      ADDRESS_HOME_STREET_ADDRESS,
      PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX,
      UNKNOWN_TYPE,
      NAME_HONORIFIC_PREFIX,
      CREDIT_CARD_EXP_2_DIGIT_YEAR,
      ADDRESS_HOME_STATE,
      UNKNOWN_TYPE,
      CREDIT_CARD_NAME_LAST,
      ACCOUNT_CREATION_PASSWORD,
      ADDRESS_HOME_HOUSE_NUMBER,
      PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX,
      CREDIT_CARD_TYPE,
      CREDIT_CARD_NAME_FULL,
      ADDRESS_HOME_APT_NUM,
      CREDIT_CARD_NAME_FIRST,
      ADDRESS_HOME_FLOOR,
      UNKNOWN_TYPE,
      ADDRESS_HOME_LANDMARK,
      UNKNOWN_TYPE,
      ADDRESS_HOME_STREET_NAME,
      ADDRESS_HOME_COUNTRY,
      CREDIT_CARD_EXP_4_DIGIT_YEAR,
      DELIVERY_INSTRUCTIONS,
      PHONE_HOME_NUMBER,
      CREDIT_CARD_VERIFICATION_CODE,
      NAME_LAST,
      CREDIT_CARD_EXP_MONTH,
      ADDRESS_HOME_OVERFLOW,
      UNKNOWN_TYPE,
      NAME_FULL,
      COMPANY_NAME,
      CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
      PHONE_HOME_CITY_AND_NUMBER,
      PHONE_HOME_CITY_CODE,
      ADDRESS_HOME_LINE2,
      ADDRESS_HOME_STREET_LOCATION,
      ADDRESS_HOME_ZIP,
      CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
      ADDRESS_HOME_OVERFLOW_AND_LANDMARK,
      ADDRESS_HOME_LINE3,
      ADDRESS_HOME_LINE1};

  // Maximum number of fields in one form that can be used as input.
  static constexpr size_t kMaxNumberOfFields = 20;

  // optimization_guide::BaseModelExecutor:
  // This function must be called on a background thread.
  // It initializes the vectorizer by reading the dictionary file
  // which can't be done on the UI thread.
  bool Preprocess(const std::vector<TfLiteTensor*>& input_tensors,
                  const FormData& input) override;
  absl::optional<std::vector<ServerFieldType>> Postprocess(
      const std::vector<const TfLiteTensor*>& output_tensors) override;

  std::unique_ptr<AutofillModelVectorizer> vectorizer_;

  // Stores the number of fields in the given to 'PreProcess()' FormData if it
  // is less than `kMaxNumberOfFields`. It will be used in `PostProcess()` to
  // return the first `fields_count_` predictions from the model.
  size_t fields_count_ = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_MODEL_EXECUTOR_H_
