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
    const std::vector<base::string16>& strings) {
  std::vector<base::StringPiece16> string_pieces;
  for (const auto& s : strings) {
    string_pieces.emplace_back(base::StringPiece16(s));
  }
  return string_pieces;
}

}  // namespace

namespace autofill {

TEST(LabelProcessingUtil, GetParseableNameStringPieces) {
  std::vector<base::string16> labels;
  labels.push_back(ASCIIToUTF16("City"));
  labels.push_back(ASCIIToUTF16("Street & House Number"));
  labels.push_back(ASCIIToUTF16(""));
  labels.push_back(ASCIIToUTF16("Zip"));

  auto expectation = base::make_optional(std::vector<base::string16>());
  expectation->push_back(ASCIIToUTF16("City"));
  expectation->push_back(ASCIIToUTF16("Street"));
  expectation->push_back(ASCIIToUTF16("House Number"));
  expectation->push_back(ASCIIToUTF16("Zip"));

  EXPECT_EQ(GetParseableLabels(StringsToStringPieces(labels)), expectation);
}

TEST(LabelProcessingUtil, GetParseableNameStringPieces_ThreeComponents) {
  std::vector<base::string16> labels;
  labels.push_back(ASCIIToUTF16("City"));
  labels.push_back(ASCIIToUTF16("Street & House Number & Floor"));
  labels.push_back(ASCIIToUTF16(""));
  labels.push_back(ASCIIToUTF16(""));
  labels.push_back(ASCIIToUTF16("Zip"));

  auto expectation = base::make_optional(std::vector<base::string16>());
  expectation->push_back(ASCIIToUTF16("City"));
  expectation->push_back(ASCIIToUTF16("Street"));
  expectation->push_back(ASCIIToUTF16("House Number"));
  expectation->push_back(ASCIIToUTF16("Floor"));
  expectation->push_back(ASCIIToUTF16("Zip"));

  EXPECT_EQ(GetParseableLabels(StringsToStringPieces(labels)), expectation);
}

TEST(LabelProcessingUtil, GetParseableNameStringPieces_TooManyComponents) {
  std::vector<base::string16> labels;
  labels.push_back(ASCIIToUTF16("City"));
  labels.push_back(ASCIIToUTF16("Street & House Number & Floor & Stairs"));
  labels.push_back(ASCIIToUTF16(""));
  labels.push_back(ASCIIToUTF16(""));
  labels.push_back(ASCIIToUTF16(""));
  labels.push_back(ASCIIToUTF16("Zip"));

  base::Optional<std::vector<base::string16>> expectation = base::nullopt;
  ;

  EXPECT_EQ(GetParseableLabels(StringsToStringPieces(labels)), expectation);
}

TEST(LabelProcessingUtil, GetParseableNameStringPieces_UnmachtingComponents) {
  std::vector<base::string16> labels;
  labels.push_back(ASCIIToUTF16("City"));
  labels.push_back(ASCIIToUTF16("Street & House Number & Floor"));
  labels.push_back(ASCIIToUTF16(""));
  labels.push_back(ASCIIToUTF16("Zip"));

  base::Optional<std::vector<base::string16>> expectation = base::nullopt;

  EXPECT_EQ(GetParseableLabels(StringsToStringPieces(labels)), expectation);
}

TEST(LabelProcessingUtil, GetParseableNameStringPieces_SplitableLabelAtEnd) {
  std::vector<base::string16> labels;
  labels.push_back(ASCIIToUTF16("City"));
  labels.push_back(ASCIIToUTF16(""));
  labels.push_back(ASCIIToUTF16("Zip"));
  labels.push_back(ASCIIToUTF16("Street & House Number & Floor"));

  base::Optional<std::vector<base::string16>> expectation = base::nullopt;

  EXPECT_EQ(GetParseableLabels(StringsToStringPieces(labels)), expectation);
}

TEST(LabelProcessingUtil, GetParseableNameStringPieces_TooLongLabel) {
  std::vector<base::string16> labels;
  labels.push_back(ASCIIToUTF16("City"));
  labels.push_back(
      ASCIIToUTF16("Street & House Number with a lot of additional text that "
                   "exceeds 40 characters by far"));
  labels.push_back(ASCIIToUTF16(""));
  labels.push_back(ASCIIToUTF16("Zip"));

  base::Optional<std::vector<base::string16>> expectation = base::nullopt;

  EXPECT_EQ(GetParseableLabels(StringsToStringPieces(labels)), expectation);
}

}  // namespace autofill
