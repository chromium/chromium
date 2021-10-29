// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/check.h"
#include "base/json/json_reader.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace policy {

namespace {

// Holds the state and performs initialization that's shared across fuzzer runs.
struct Environment {
  Environment() {
    schema = Schema::Wrap(GetChromeSchemaData());
    CHECK(schema.valid());
  }

  Schema schema;
};

}  // namespace

// Fuzzer for methods of the `Schema` class.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;

  absl::optional<base::Value> parsed_json = base::JSONReader::Read(
      base::StringPiece(reinterpret_cast<const char*>(data), size));
  if (!parsed_json)
    return 0;

  // Exercise validation/normalization with every possible strategy.
  for (auto strategy : {SCHEMA_STRICT, SCHEMA_ALLOW_UNKNOWN,
                        SCHEMA_ALLOW_UNKNOWN_AND_INVALID_LIST_ENTRY}) {
    env.schema.Validate(*parsed_json, strategy,
                        /*out_error_path=*/nullptr,
                        /*out_error=*/nullptr);

    base::Value copy = parsed_json->Clone();
    if (env.schema.Normalize(&copy, strategy,
                             /*out_error_path=*/nullptr,
                             /*out_error=*/nullptr, /*out_changed=*/nullptr)) {
      // If normalization succeeded, the validation of the result should succeed
      // too.
      CHECK(env.schema.Validate(copy, strategy,
                                /*out_error_path=*/nullptr,
                                /*out_error=*/nullptr));
    }
  }

  // Exercise the sensitive masking logic.
  base::Value copy2 = parsed_json->Clone();
  env.schema.MaskSensitiveValues(&copy2);

  return 0;
}

}  // namespace policy
