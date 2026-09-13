// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/system/name_value_pairs_parser.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/containers/span.h"
#include "base/strings/string_view_util.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

namespace ash::system {

// We need a class that can be friend of NameValuePairsParser because we fuzz
// input to private methods that underpin the public methods.
class NameValuePairsParserFuzzer {
 public:
  void testOneInput(base::span<const uint8_t> data) {
    const std::string input(base::as_string_view(data));

    name_value_map_.clear();

    testInputAsVpdDumpLine(input);
    testInputAsCrossystemOutputLine(input);

    testInputAsVpdDumpValuesForKey(input);
  }

 private:
  void testInputAsVpdDumpLine(const std::string& input) {
    NameValuePairsParser parser(&name_value_map_);
    parser.ParseNameValuePairs(input, NameValuePairsFormat::kVpdDump);
  }

  void testInputAsCrossystemOutputLine(const std::string& input) {
    NameValuePairsParser parser(&name_value_map_);
    parser.ParseNameValuePairs(input, NameValuePairsFormat::kCrossystem);
  }

  void testInputAsVpdDumpValuesForKey(const std::string& input) {
    // Test with the input as is as a value (which may be malformed due to
    // the presence of newlines in it).
    testInputAsVpdDumpValueForKey(input);

    // Test with the input as a value on the same line (i.e., without any
    // newline in it).
    std::string value = input;
    std::erase(value, '\n');
    testInputAsVpdDumpValueForKey(value);
    // TODO(crbug.com/40197992): Check that the value for "key" is |value|.
  }

  void testInputAsVpdDumpValueForKey(const std::string& input) {
    name_value_map_.erase("key");
    testInputAsVpdDumpLine(std::string("\"key\"=\"") + input + "\"\n");
  }

  NameValuePairsParser::NameValueMap name_value_map_;
};

}  // namespace ash::system

DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(const base::span<const uint8_t> bytes) {
  ash::system::NameValuePairsParserFuzzer fuzzer;
  fuzzer.testOneInput(bytes);
  return 0;
}
