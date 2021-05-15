// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_processing/label_processing_util.h"

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace {

std::vector<base::StringPiece16> StringsToStringPieces(
    const std::vector<std::u16string>& strings) {
  std::vector<base::StringPiece16> string_pieces;
  for (const auto& s : strings) {
    string_pieces.emplace_back(base::StringPiece16(s));
  }
  return string_pieces;
}

}  // namespace

namespace autofill {

TEST(LabelProcessingUtil, GetParseableNameStringPieces) {
  std::vector<std::u16string> labels;
  labels.push_back(u"City");
  labels.push_back(u"Street & House Number");
  labels.push_back(u"");
  labels.push_back(u"Zip");

  auto expectation = absl::make_optional(std::vector<std::u16string>());
  expectation->push_back(u"City");
  expectation->push_back(u"Street");
  expectation->push_back(u"House Number");
  expectation->push_back(u"Zip");

  EXPECT_EQ(GetParseableLabels(StringsToStringPieces(labels)), expectation);
}

TEST(LabelProcessingUtil, GetParseableNameStringPieces_ThreeComponents) {
  std::vector<std::u16string> labels;
  labels.push_back(u"City");
  labels.push_back(u"Street & House Number & Floor");
  labels.push_back(u"");
  labels.push_back(u"");
  labels.push_back(u"Zip");

  auto expectation = absl::make_optional(std::vector<std::u16string>());
  expectation->push_back(u"City");
  expectation->push_back(u"Street");
  expectation->push_back(u"House Number");
  expectation->push_back(u"Floor");
  expectation->push_back(u"Zip");

  EXPECT_EQ(GetParseableLabels(StringsToStringPieces(labels)), expectation);
}

TEST(LabelProcessingUtil, GetParseableNameStringPieces_TooManyComponents) {
  std::vector<std::u16string> labels;
  labels.push_back(u"City");
  labels.push_back(u"Street & House Number & Floor & Stairs");
  labels.push_back(u"");
  labels.push_back(u"");
  labels.push_back(u"");
  labels.push_back(u"Zip");

  absl::optional<std::vector<std::u16string>> expectation = absl::nullopt;
  ;

  EXPECT_EQ(GetParseableLabels(StringsToStringPieces(labels)), expectation);
}

TEST(LabelProcessingUtil, GetParseableNameStringPieces_UnmachtingComponents) {
  std::vector<std::u16string> labels;
  labels.push_back(u"City");
  labels.push_back(u"Street & House Number & Floor");
  labels.push_back(u"");
  labels.push_back(u"Zip");

  absl::optional<std::vector<std::u16string>> expectation = absl::nullopt;

  EXPECT_EQ(GetParseableLabels(StringsToStringPieces(labels)), expectation);
}

TEST(LabelProcessingUtil, GetParseableNameStringPieces_SplitableLabelAtEnd) {
  std::vector<std::u16string> labels;
  labels.push_back(u"City");
  labels.push_back(u"");
  labels.push_back(u"Zip");
  labels.push_back(u"Street & House Number & Floor");

  absl::optional<std::vector<std::u16string>> expectation = absl::nullopt;

  EXPECT_EQ(GetParseableLabels(StringsToStringPieces(labels)), expectation);
}

TEST(LabelProcessingUtil, GetParseableNameStringPieces_TooLongLabel) {
  std::vector<std::u16string> labels;
  labels.push_back(u"City");
  labels.push_back(
      u"Street & House Number with a lot of additional text that exceeds 40 "
      u"characters by far");
  labels.push_back(u"");
  labels.push_back(u"Zip");

  absl::optional<std::vector<std::u16string>> expectation = absl::nullopt;

  EXPECT_EQ(GetParseableLabels(StringsToStringPieces(labels)), expectation);
}

}  // namespace autofill
