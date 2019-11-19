// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CHROME_CLEANER_TEST_TEST_NAME_HELPER_H_
#define COMPONENTS_CHROME_CLEANER_TEST_TEST_NAME_HELPER_H_

#include <string>

#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

// A functor that formats test parameters for use in a test name. Use this
// instead of PrintToStringParamName, which sometimes returns characters that
// aren't valid in test names.
//
// Known limitations:
//
// * A char* initialized with "string value" is formatted as
// ADDR_pointer_to_string_value. To avoid this, declare the parameter with type
// std::string. Example: instead of "TestWithParam<std::tuple<const char*,
// bool>>" use "TestWithParam<std::tuple<std::string, bool>>".
//
// * A string initialized with the literal L"wide string" is formatted as
// Lwide_string. No known workaround.
//
// Note that long test names can cause problems because they can be used to
// generate file names, which have a max length of 255 on Windows. So use this
// judiciously.
struct GetParamNameForTest {
  template <typename ParamType>
  std::string operator()(
      const ::testing::TestParamInfo<ParamType>& info) const {
    std::string param_name = ::testing::PrintToString(info.param);

    // Remove or convert invalid characters that are inserted by PrintToString:
    //
    // * Strings formatted as "string value" (including quotes) -> string_value
    // * Tuples formatted as (value1, value2) -> value1_value2
    // * Mojo enums formatted as Enum::VALUE -> EnumVALUE
    base::RemoveChars(param_name, "\"(),:", &param_name);
    base::ReplaceChars(param_name, " ", "_", &param_name);

    return param_name;
  }
};

}  // namespace chrome_cleaner

#endif  // COMPONENTS_CHROME_CLEANER_TEST_TEST_NAME_HELPER_H_
