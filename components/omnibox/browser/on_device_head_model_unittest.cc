// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/on_device_head_model.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::Pair;

namespace {

// The test head model used for unittests contains 14 queries and their scores
// shown below; the test model uses 3-bytes address and 2-bytes score so the
// highest score is 32767:
// ----------------------
// Query            Score
// ----------------------
// g                32767
// gmail            32766
// google maps      32765
// google           32764
// get out          32763
// googler          32762
// gamestop         32761
// maps             32761
// mail             32760
// map              32759
// 谷歌              32759
// ガツガツしてる人    32759
// 비데 두꺼비         32759
// переводчик       32759
// ----------------------
// The tree structure for queries above is similar as this:
//  [ g | ma | 谷歌 | ガツガツしてる人| 비데 두꺼비 | переводчик ]
//    |   |
//    | [ p | il ]
//    |   |
//    | [ # | s ]
//    |
//  [ # | oogle | mail | et out | amestop ]
//          |
//        [ # | _maps | er ]

base::FilePath GetTestModelPath() {
  base::FilePath file_path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &file_path);
  file_path = file_path.AppendASCII(
      "components/test/data/omnibox/on_device_head_test_model_index.bin");
  return file_path;
}

}  // namespace

class OnDeviceHeadModelTest : public testing::Test {
 protected:
  void SetUp() override {
    base::FilePath file_path = GetTestModelPath();
    ASSERT_TRUE(base::PathExists(file_path));
#if BUILDFLAG(IS_WIN)
    model_filename_ = base::WideToUTF8(file_path.value());
#else
    model_filename_ = file_path.value();
#endif
    ASSERT_FALSE(model_filename_.empty());
  }

  void TearDown() override { model_filename_.clear(); }

  std::string model_filename_;
};

TEST_F(OnDeviceHeadModelTest, GetSuggestions) {
  auto suggestions =
      OnDeviceHeadModel::GetSuggestionsForPrefix(model_filename_, 4, "go");
  EXPECT_THAT(suggestions,
              ElementsAre(Pair("google maps", 32765), Pair("google", 32764),
                          Pair("googler", 32762)));

  suggestions =
      OnDeviceHeadModel::GetSuggestionsForPrefix(model_filename_, 4, "ge");
  EXPECT_THAT(suggestions, ElementsAre(Pair("get out", 32763)));

  suggestions =
      OnDeviceHeadModel::GetSuggestionsForPrefix(model_filename_, 4, "ga");
  EXPECT_THAT(suggestions, ElementsAre(Pair("gamestop", 32761)));
}

TEST_F(OnDeviceHeadModelTest, NoMatch) {
  auto suggestions =
      OnDeviceHeadModel::GetSuggestionsForPrefix(model_filename_, 4, "x");
  EXPECT_TRUE(suggestions.empty());
}

TEST_F(OnDeviceHeadModelTest, MatchTheEndOfSuggestion) {
  auto suggestions =
      OnDeviceHeadModel::GetSuggestionsForPrefix(model_filename_, 4, "ap");
  EXPECT_TRUE(suggestions.empty());
}

TEST_F(OnDeviceHeadModelTest, MatchAtTheMiddleOfSuggestion) {
  auto suggestions =
      OnDeviceHeadModel::GetSuggestionsForPrefix(model_filename_, 4, "st");
  EXPECT_TRUE(suggestions.empty());
}

TEST_F(OnDeviceHeadModelTest, EmptyInput) {
  auto suggestions =
      OnDeviceHeadModel::GetSuggestionsForPrefix(model_filename_, 4, "");
  EXPECT_TRUE(suggestions.empty());
}

TEST_F(OnDeviceHeadModelTest, SetMaxSuggestionsToReturn) {
  auto suggestions =
      OnDeviceHeadModel::GetSuggestionsForPrefix(model_filename_, 5, "g");
  EXPECT_THAT(suggestions,
              ElementsAre(Pair("g", 32767), Pair("gmail", 32766),
                          Pair("google maps", 32765), Pair("google", 32764),
                          Pair("get out", 32763)));

  suggestions =
      OnDeviceHeadModel::GetSuggestionsForPrefix(model_filename_, 2, "ma");
  EXPECT_THAT(suggestions,
              ElementsAre(Pair("maps", 32761), Pair("mail", 32760)));
}

TEST_F(OnDeviceHeadModelTest, NonEnglishLanguage) {
  // Chinese.
  auto suggestions =
      OnDeviceHeadModel::GetSuggestionsForPrefix(model_filename_, 4, "谷");
  EXPECT_THAT(suggestions, ElementsAre(Pair("谷歌", 32759)));

  // Japanese.
  suggestions = OnDeviceHeadModel::GetSuggestionsForPrefix(model_filename_, 4,
                                                           "ガツガツ");
  EXPECT_THAT(suggestions, ElementsAre(Pair("ガツガツしてる人", 32759)));

  // Korean.
  suggestions =
      OnDeviceHeadModel::GetSuggestionsForPrefix(model_filename_, 4, "비데 ");
  EXPECT_THAT(suggestions, ElementsAre(Pair("비데 두꺼비", 32759)));

  // Russian.
  suggestions =
      OnDeviceHeadModel::GetSuggestionsForPrefix(model_filename_, 4, "пере");
  EXPECT_THAT(suggestions, ElementsAre(Pair("переводчик", 32759)));
}
