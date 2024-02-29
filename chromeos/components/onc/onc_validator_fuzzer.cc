// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/onc/onc_validator.h"

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string_view>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/values.h"
#include "chromeos/components/onc/onc_signature.h"

namespace chromeos {
namespace onc {

namespace {

// Holds the state and performs initialization that's shared across fuzzer runs.
struct Environment {
  Environment() {
    // Prevent spamming stdout with ONC validation errors.
    logging::SetMinLogLevel(logging::LOGGING_FATAL);
  }
};

}  // namespace

// Fuzzer for methods of the `Validator` class.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;

  std::optional<base::Value> parsed_json = base::JSONReader::Read(
      std::string_view(reinterpret_cast<const char*>(data), size));
  if (!parsed_json || !parsed_json->is_dict())
    return 0;

  for (bool error_on_unknown_field : {false, true}) {
    for (bool error_on_wrong_recommended : {false, true}) {
      for (bool error_on_missing_field : {false, true}) {
        for (bool managed_onc : {false, true}) {
          Validator validator(
              error_on_unknown_field, error_on_wrong_recommended,
              error_on_missing_field, managed_onc, /*log_warnings=*/false);
          Validator::Result validation_result;
          validator.ValidateAndRepairObject(&kNetworkConfigurationSignature,
                                            parsed_json->GetDict(),
                                            &validation_result);
        }
      }
    }
  }

  return 0;
}

}  // namespace onc
}  // namespace chromeos
