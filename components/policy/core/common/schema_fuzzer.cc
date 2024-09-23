// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/schema.h"

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string>

#include "base/check.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "components/policy/policy_constants.h"

namespace policy {

namespace {

// Holds the state and performs initialization that's shared across fuzzer runs.
struct Environment {
  Environment() {
    chrome_policy_schema = Schema::Wrap(GetChromeSchemaData());
    CHECK(chrome_policy_schema.valid());
  }

  Schema chrome_policy_schema;
};

// Test schema parsing.
void TestParsing(const Environment& env, const std::string& data) {
  std::ignore = Schema::Parse(data);
}

// Test validation and normalization against the Chrome policy schema.
void TestValidation(const Environment& env, const base::Value& parsed_json) {
  // Exercise with every possible strategy.
  for (auto strategy : {SCHEMA_STRICT, SCHEMA_ALLOW_UNKNOWN,
                        SCHEMA_ALLOW_UNKNOWN_AND_INVALID_LIST_ENTRY,
                        SCHEMA_ALLOW_UNKNOWN_WITHOUT_WARNING}) {
    env.chrome_policy_schema.Validate(parsed_json, strategy,
                                      /*out_error_path=*/nullptr,
                                      /*out_error=*/nullptr);

    base::Value copy = parsed_json.Clone();
    if (env.chrome_policy_schema.Normalize(&copy, strategy,
                                           /*out_error_path=*/nullptr,
                                           /*out_error=*/nullptr,
                                           /*out_changed=*/nullptr)) {
      // If normalization succeeded, the validation of the result should succeed
      // too.
      CHECK(env.chrome_policy_schema.Validate(copy, strategy,
                                              /*out_error_path=*/nullptr,
                                              /*out_error=*/nullptr));
    }
  }
}

// Test masking sensitive values.
void TestMasking(const Environment& env, const base::Value& parsed_json) {
  base::Value copy = parsed_json.Clone();
  env.chrome_policy_schema.MaskSensitiveValues(&copy);
}

}  // namespace

// Fuzzer for methods of the `Schema` class.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;

  const std::string data_string(reinterpret_cast<const char*>(data), size);

  TestParsing(env, data_string);

  std::optional<base::Value> parsed_json = base::JSONReader::Read(data_string);
  if (parsed_json) {
    TestValidation(env, *parsed_json);
    TestMasking(env, *parsed_json);
  }

  return 0;
}

}  // namespace policy
