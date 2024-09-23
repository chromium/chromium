// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "chromeos/ash/components/system/name_value_pairs_parser.h"

namespace ash::system {

// We need a class that can be friend of NameValuePairsParser because we fuzz
// input to private methods that underpin the public methods.
class NameValuePairsParserFuzzer {
 public:
  void testOneInput(const uint8_t* data, size_t size) {
    const std::string input = std::string(data, data + size);

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
    value.erase(std::remove(value.begin(), value.end(), '\n'), value.end());
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

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  ash::system::NameValuePairsParserFuzzer fuzzer;
  fuzzer.testOneInput(data, size);
  return 0;
}
