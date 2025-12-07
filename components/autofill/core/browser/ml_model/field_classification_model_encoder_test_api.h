#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_FIELD_CLASSIFICATION_MODEL_ENCODER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_FIELD_CLASSIFICATION_MODEL_ENCODER_TEST_API_H_

#include <string>
#include <string_view>

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/ml_model/field_classification_model_encoder.h"

namespace autofill {

// Test API for `FieldClassificationModelEncoder`.
class FieldClassificationModelEncoderTestApi {
 public:
  explicit FieldClassificationModelEncoderTestApi(
      FieldClassificationModelEncoder* encoder)
      : encoder_(*encoder) {}

  std::u16string StandardizeString(std::u16string_view input) const {
    return encoder_->StandardizeString(input);
  }

 private:
  const raw_ref<FieldClassificationModelEncoder> encoder_;
};

inline FieldClassificationModelEncoderTestApi test_api(
    FieldClassificationModelEncoder& encoder) {
  return FieldClassificationModelEncoderTestApi(&encoder);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_FIELD_CLASSIFICATION_MODEL_ENCODER_TEST_API_H_
