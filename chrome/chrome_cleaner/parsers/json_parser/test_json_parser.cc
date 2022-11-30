// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/parsers/json_parser/test_json_parser.h"

#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/chrome_cleaner/parsers/target/parser_impl.h"

namespace chrome_cleaner {

void TestJsonParser::Parse(const std::string& json,
                           ParseDoneCallback callback) {
  auto value_with_error = base::JSONReader::ReadAndReturnValueWithError(
      json, base::JSON_PARSE_CHROMIUM_EXTENSIONS |
                base::JSON_ALLOW_TRAILING_COMMAS |
                base::JSON_REPLACE_INVALID_CHARACTERS);
  if (value_with_error.has_value()) {
    std::move(callback).Run(std::move(*value_with_error), absl::nullopt);
  } else {
    std::move(callback).Run(
        absl::nullopt, absl::make_optional(value_with_error.error().message));
  }
}

}  // namespace chrome_cleaner
