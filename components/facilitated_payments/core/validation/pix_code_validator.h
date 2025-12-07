// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_VALIDATION_PIX_CODE_VALIDATOR_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_VALIDATION_PIX_CODE_VALIDATOR_H_

#include <string>
#include <utility>

#include "base/functional/callback_forward.h"
#include "components/facilitated_payments/core/mojom/pix_code_validator.mojom.h"

namespace payments::facilitated {

class PixCodeValidator : public mojom::PixCodeValidator {
 public:
  PixCodeValidator();
  ~PixCodeValidator() override;

  PixCodeValidator& operator=(const PixCodeValidator& other) = delete;
  PixCodeValidator(const PixCodeValidator& other) = delete;

  // Returns true if the input `code` is a valid Pix code, i.e.:
  // 1) It consists of valid Pix code sections.
  // 2) The first section is the payload format indicator.
  // 3) The last section is the CRC16.
  // 4) The merchant account information section contains valid subsections,
  //    including the Pix code indicator as the first subsection. The case
  //    (upper/lower) of the Pix code indicator is ignored.
  // 5) The additional data field template section, if present, contains valid
  //    subsections.
  //
  // This function does not validate the value of the CRC16.
  static mojom::PixQrCodeType GetPixQrCodeType(std::string_view code);

  // Returns true if the input `code` contains the Pix code indicator.
  static bool ContainsPixIdentifier(std::string_view code);

 private:
  // mojom::PixValidator implementation:
  void ValidatePixCode(
      const std::string& input_text,
      base::OnceCallback<void(std::optional<mojom::PixQrCodeType>)> callback)
      override;
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_VALIDATION_PIX_CODE_VALIDATOR_H_
