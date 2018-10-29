// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_RANDOMIZED_ENCODER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_RANDOMIZED_ENCODER_H_

#include <string>

#include "base/strings/string_piece.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/common/signatures_util.h"

namespace autofill {

// Encodes string values using the differential-privacy scheme as described
// in go/autofill-metadata-upload (Google internal link).
class RandomizedEncoder {
 public:
  struct EncodingInfo;

  RandomizedEncoder(std::string seed,
                    AutofillRandomizedValue_EncodingType encoding_type);

  // Encode |data_value| using this instance's |encoding_type_|.
  std::string Encode(FormSignature form_signature,
                     FieldSignature field_signature,
                     base::StringPiece data_type,
                     base::StringPiece data_value) const;

 protected:
  // Get the pseudo-random string to use at the coin bit-field. This function
  // is internal, but exposed here to facilitate testing.
  std::string GetCoins(FormSignature form_signature,
                       FieldSignature field_signature,
                       base::StringPiece data_type) const;

  // Get the pseudo-random string to use at the noise bit-field. This function
  // is internal, but exposed here to facilitate testing.
  std::string GetNoise(FormSignature form_signature,
                       FieldSignature field_signature,
                       base::StringPiece data_type) const;

 private:
  const std::string seed_;
  const EncodingInfo* const encoding_info_;
};
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_RANDOMIZED_ENCODER_H_
