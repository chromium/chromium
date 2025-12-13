// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/onc/onc_normalizer.h"

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string_view>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "chromeos/components/onc/onc_signature.h"

namespace ash::onc {

// Fuzzer for methods of the `Normalizer` class.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::optional<base::Value::Dict> parsed_json = base::JSONReader::ReadDict(
      std::string_view(reinterpret_cast<const char*>(data), size),
      base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!parsed_json) {
    return 0;
  }

  for (bool remove_recommended_fields : {false, true}) {
    Normalizer normalizer(remove_recommended_fields);
    normalizer.NormalizeObject(&chromeos::onc::kNetworkConfigurationSignature,
                               *parsed_json);
  }

  return 0;
}

}  // namespace ash::onc
