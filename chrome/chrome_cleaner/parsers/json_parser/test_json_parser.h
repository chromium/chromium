// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_PARSERS_JSON_PARSER_TEST_JSON_PARSER_H_
#define CHROME_CHROME_CLEANER_PARSERS_JSON_PARSER_TEST_JSON_PARSER_H_

#include "chrome/chrome_cleaner/parsers/json_parser/json_parser_api.h"

namespace chrome_cleaner {

// An implementation of JsonParserAPI for testing that directly calls
// base::JSONReader and runs the callback with the result. This should only be
// used for tests where a JsonParserAPI is needed.
class TestJsonParser : public JsonParserAPI {
 public:
  void Parse(const std::string& json, ParseDoneCallback callback) override;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_PARSERS_JSON_PARSER_TEST_JSON_PARSER_H_
