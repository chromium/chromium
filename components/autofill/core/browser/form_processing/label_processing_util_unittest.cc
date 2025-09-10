// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_processing/label_processing_util.h"

#include <string_view>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;

TEST(LabelProcessingUtil, GetParseableNameLabels) {
  std::vector<std::u16string_view> labels = {u"City", u"Street & House Number",
                                             u"", u"Zip"};
  EXPECT_THAT(GetParseableLabelsForTest(labels),
              ElementsAre(u"City", u"Street", u"House Number", u"Zip"));

  // The label is also split when consecutive fields share the same label.
  labels[2] = labels[1];
  EXPECT_THAT(GetParseableLabelsForTest(labels),
              ElementsAre(u"City", u"Street", u"House Number", u"Zip"));
}

TEST(LabelProcessingUtil, GetParseableNameLabels_ThreeComponents) {
  EXPECT_THAT(
      GetParseableLabelsForTest(
          {u"City", u"Street & House Number & Floor", u"", u"", u"Zip"}),
      ElementsAre(u"City", u"Street", u"House Number", u"Floor", u"Zip"));
}

TEST(LabelProcessingUtil, GetParseableNameLabels_TooManyComponents) {
  std::vector<std::u16string_view> labels = {
      u"City", u"Street & House Number & Floor & Stairs", u"", u"", u"",
      u"Zip"};
  EXPECT_EQ(GetParseableLabelsForTest(labels), labels);
}

TEST(LabelProcessingUtil, GetParseableNameLabels_UnmachtingComponents) {
  std::vector<std::u16string_view> labels = {
      u"City", u"Street & House Number & Floor", u"", u"Zip"};
  EXPECT_EQ(GetParseableLabelsForTest(labels), labels);
}

TEST(LabelProcessingUtil, GetParseableNameLabels_SplitableLabelAtEnd) {
  std::vector<std::u16string_view> labels = {u"City", u"", u"Zip",
                                             u"Street & House Number & Floor"};
  EXPECT_EQ(GetParseableLabelsForTest(labels), labels);
}

TEST(LabelProcessingUtil, GetParseableNameLabels_TooLongLabel) {
  std::vector<std::u16string_view> labels = {
      u"City",
      u"Street & House Number with a lot of additional text that exceeds 40 "
      u"characters by far",
      u"", u"Zip"};
  EXPECT_EQ(GetParseableLabelsForTest(labels), labels);
}

// Tests that (only) if kAutofillEnableSupportForParsingWithSharedLabels is
// enabled, GetParseableLabels() returns the labels that are different to
// AutofillField::label().
TEST(LabelProcessingUtil, GetParseableLabels_Feature) {
  test::AutofillUnitTestEnvironment autofill_environment;
  std::vector<std::unique_ptr<AutofillField>> fields;
  for (const char* label :
       {"City", "Street & House Number & Floor", "", "", "Zip"}) {
    fields.push_back(std::make_unique<AutofillField>(test::CreateTestFormField(
        /*label=*/label, /*name=*/"",
        /*value=*/"", /*type=*/FormControlType::kInputText)));
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        features::kAutofillEnableSupportForParsingWithSharedLabels);
    EXPECT_THAT(GetParseableLabels(fields), IsEmpty());
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        features::kAutofillEnableSupportForParsingWithSharedLabels);
    EXPECT_THAT(GetParseableLabels(fields),
                ElementsAre(Pair(fields[1]->global_id(), u"Street"),
                            Pair(fields[2]->global_id(), u"House Number"),
                            Pair(fields[3]->global_id(), u"Floor")));
  }
}

}  // namespace

}  // namespace autofill
