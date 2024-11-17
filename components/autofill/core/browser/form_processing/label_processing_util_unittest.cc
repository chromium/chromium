// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_processing/label_processing_util.h"

#include <string_view>
#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using ::testing::ElementsAre;

TEST(LabelProcessingUtil, GetParseableNameLabels) {
  std::vector<std::u16string_view> labels = {u"City", u"Street & House Number",
                                             u"", u"Zip"};
  EXPECT_THAT(GetParseableLabels(labels),
              ElementsAre(u"City", u"Street", u"House Number", u"Zip"));

  // The label is also split when consecutive fields share the same label.
  labels[2] = labels[1];
  EXPECT_THAT(GetParseableLabels(labels),
              ElementsAre(u"City", u"Street", u"House Number", u"Zip"));
}

TEST(LabelProcessingUtil, GetParseableNameLabels_ThreeComponents) {
  EXPECT_THAT(
      GetParseableLabels(
          {u"City", u"Street & House Number & Floor", u"", u"", u"Zip"}),
      ElementsAre(u"City", u"Street", u"House Number", u"Floor", u"Zip"));
}

TEST(LabelProcessingUtil, GetParseableNameLabels_TooManyComponents) {
  std::vector<std::u16string_view> labels = {
      u"City", u"Street & House Number & Floor & Stairs", u"", u"", u"",
      u"Zip"};
  EXPECT_EQ(GetParseableLabels(labels), labels);
}

TEST(LabelProcessingUtil, GetParseableNameLabels_UnmachtingComponents) {
  std::vector<std::u16string_view> labels = {
      u"City", u"Street & House Number & Floor", u"", u"Zip"};
  EXPECT_EQ(GetParseableLabels(labels), labels);
}

TEST(LabelProcessingUtil, GetParseableNameLabels_SplitableLabelAtEnd) {
  std::vector<std::u16string_view> labels = {u"City", u"", u"Zip",
                                             u"Street & House Number & Floor"};
  EXPECT_EQ(GetParseableLabels(labels), labels);
}

TEST(LabelProcessingUtil, GetParseableNameLabels_TooLongLabel) {
  std::vector<std::u16string_view> labels = {
      u"City",
      u"Street & House Number with a lot of additional text that exceeds 40 "
      u"characters by far",
      u"", u"Zip"};
  EXPECT_EQ(GetParseableLabels(labels), labels);
}

}  // namespace

}  // namespace autofill
