// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hats/hats_service_desktop.h"

#include <utility>

#include "components/version_info/version_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AnyOf;
using testing::Eq;
using testing::Pair;
using testing::UnorderedElementsAre;

class HatsServiceDesktopProductSpecificDataTest : public testing::Test {
 protected:
  static SurveyStringData AddDefaultProductSpecificData(
      SurveyStringData product_specific_string_data) {
    return HatsServiceDesktop::AddDefaultProductSpecificData(
        std::move(product_specific_string_data));
  }
};

TEST_F(HatsServiceDesktopProductSpecificDataTest, AddsDefaults) {
  SurveyStringData result =
      AddDefaultProductSpecificData({{"String Field", "Value"}});

  EXPECT_THAT(result, UnorderedElementsAre(
                          Pair("String Field", "Value"),
                          Pair(HatsServiceDesktop::kChromeChannelPsdKey,
                               AnyOf(Eq("Stable"), Eq("Beta"), Eq("Dev"),
                                     Eq("Canary"), Eq("Local"))),
                          Pair(HatsServiceDesktop::kChromeVersionPsdKey,
                               version_info::GetVersionNumber())));
}

TEST_F(HatsServiceDesktopProductSpecificDataTest, AddsDefaultsToEmptyData) {
  SurveyStringData result = AddDefaultProductSpecificData({});

  EXPECT_THAT(result,
              UnorderedElementsAre(
                  Pair(HatsServiceDesktop::kChromeChannelPsdKey, testing::_),
                  Pair(HatsServiceDesktop::kChromeVersionPsdKey,
                       version_info::GetVersionNumber())));
}
