// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/json/json_reader.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "chromeos/components/onc/onc_signature.h"
#include "chromeos/network/onc/onc_normalizer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {
namespace onc {

// Fuzzer for methods of the `Normalizer` class.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  absl::optional<base::Value> parsed_json = base::JSONReader::Read(
      base::StringPiece(reinterpret_cast<const char*>(data), size));
  if (!parsed_json || !parsed_json->is_dict())
    return 0;

  for (bool remove_recommended_fields : {false, true}) {
    Normalizer normalizer(remove_recommended_fields);
    normalizer.NormalizeObject(&kNetworkConfigurationSignature, *parsed_json);
  }

  return 0;
}

}  // namespace onc
}  // namespace chromeos
