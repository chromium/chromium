// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/remove_app_compat_entries.h"

#include <string>
#include <string_view>
#include <tuple>

#include "testing/gtest/include/gtest/gtest.h"

// Tests inputs that contain no tokens to remove.
TEST(RemoveCompatLayersTest, None) {
  static constexpr std::wstring_view kInputs[] = {
      L"",
      L"see the wind",
      L"fuzzWIN98buzz",
  };
  for (const auto& input : kInputs) {
    std::wstring layers(input);
    EXPECT_FALSE(RemoveCompatLayers(layers));
    EXPECT_EQ(layers, input);
  }
}

// Tests inputs for which all tokens are to be removed.
TEST(RemoveCompatLayersTest, All) {
  static constexpr std::wstring_view kInputs[] = {
      L"NT4SP5",
      L"NT4SP5 VISTARTM VISTASP1 VISTASP2 WIN10RTM WIN2000 WIN4SP5 WIN7RTM "
      L"WIN7SP1 WIN8RTM WIN81RTM WIN95 WIN98 WINSRV03SP1 WINSRV08SP1 "
      L"WINSRV16RTM WINSRV19RTM WINXPSP2 WINXPSP3",
  };
  for (const auto& input : kInputs) {
    std::wstring layers(input);
    EXPECT_TRUE(RemoveCompatLayers(layers));
    EXPECT_TRUE(layers.empty());
  }
}

// Tests inputs for which a subset of tokens are to be removed.
TEST(RemoveCompatLayersTest, Some) {
  static constexpr std::tuple<std::wstring_view, std::wstring_view> kInputs[] =
      {
          {L"~", L""},
          {L"~ NT4SP5", L""},
          {L"~ NT4SP5 VISTARTM VISTASP1 VISTASP2 WIN10RTM WIN2000 WIN4SP5 "
           L"WIN7RTM "
           L"WIN7SP1 WIN8RTM WIN81RTM WIN95 WIN98 WINSRV03SP1 WINSRV08SP1 "
           L"WINSRV16RTM WINSRV19RTM WINXPSP2 WINXPSP3",
           L""},
          {L"~ something NT4SP5 rotten", L"~ something rotten"},
          {L"~ NT4SP5 i love you", L"~ i love you"},
          {L"~ dont leave me VISTARTM", L"~ dont leave me"},
      };
  for (const auto& [input, result] : kInputs) {
    std::wstring layers(input);
    EXPECT_TRUE(RemoveCompatLayers(layers));
    EXPECT_EQ(layers, result);
  }
}
