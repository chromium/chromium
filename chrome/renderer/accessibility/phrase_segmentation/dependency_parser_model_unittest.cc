// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/phrase_segmentation/dependency_parser_model.h"

#include <array>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

base::File CreateInvalidModelFile() {
  base::ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path =
      temp_dir.GetPath().AppendASCII("model_file.tflite");
  base::File file(file_path, (base::File::FLAG_CREATE | base::File::FLAG_READ |
                              base::File::FLAG_WRITE |
                              base::File::FLAG_CAN_DELETE_ON_CLOSE));
  EXPECT_TRUE(
      file.WriteAtCurrentPosAndCheck(base::byte_span_from_cstring("12345")));
  return file;
}

base::File GetValidModelFile() {
  base::FilePath source_root_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);
  base::FilePath model_file_path = source_root_dir.AppendASCII("chrome")
                                       .AppendASCII("test")
                                       .AppendASCII("data")
                                       .AppendASCII("accessibility")
                                       .AppendASCII("phrase_segmentation")
                                       .AppendASCII("model.tflite");
  base::File file(model_file_path,
                  (base::File::FLAG_OPEN | base::File::FLAG_READ));
  return file;
}

std::unique_ptr<DependencyParserModel> GetValidDependencyParserModel() {
  auto instance = std::make_unique<DependencyParserModel>();
  if (!instance->IsAvailable()) {
    base::File file = GetValidModelFile();
    instance->UpdateWithFile(std::move(file));
  }
  EXPECT_TRUE(instance->IsAvailable());
  return instance;
}

class DependencyParserModelTest : public testing::Test {
 protected:
  base::HistogramTester histogram_tester_;
};

TEST_F(DependencyParserModelTest, ModelUnavailable) {
  DependencyParserModel dependency_parser_model;
  EXPECT_FALSE(dependency_parser_model.IsAvailable());
  EXPECT_THAT(dependency_parser_model.GetPhraseBoundaries(u"Hello world."),
              ::testing::IsEmpty());
}

TEST_F(DependencyParserModelTest,
       GetPhraseBoundariesForSentencesModelUnavailable) {
  DependencyParserModel dependency_parser_model;
  EXPECT_FALSE(dependency_parser_model.IsAvailable());
  // The result should still have one (empty) entry per input sentence.
  EXPECT_THAT(
      dependency_parser_model.GetPhraseBoundariesForSentences(
          {u"Hello world.", u"Goodbye world."}),
      ::testing::ElementsAre(::testing::IsEmpty(), ::testing::IsEmpty()));
}

TEST_F(DependencyParserModelTest, EmptyFileProvided) {
  DependencyParserModel dependency_parser_model;
  dependency_parser_model.UpdateWithFile(base::File());

  EXPECT_FALSE(dependency_parser_model.IsAvailable());
  histogram_tester_.ExpectUniqueSample(
      "Accessibility.DependencyParserModel.DependencyParserModelState",
      DependencyParserModelState::kModelFileInvalid, 1);
}

TEST_F(DependencyParserModelTest, UnsupportedModelFileProvided) {
  base::File file = CreateInvalidModelFile();
  DependencyParserModel dependency_parser_model;
  dependency_parser_model.UpdateWithFile(std::move(file));
  EXPECT_FALSE(dependency_parser_model.IsAvailable());
  histogram_tester_.ExpectUniqueSample(
      "Accessibility.DependencyParserModel.DependencyParserModelState",
      DependencyParserModelState::kModelFileInvalid, 1);
  histogram_tester_.ExpectUniqueSample(
      "Accessibility.DependencyParserModel.InvalidModelFile", true, 1);
  histogram_tester_.ExpectTotalCount(
      "Accessibility.DependencyParserModel.Create.Duration", 0);
}

class DependencyParserModelValidTest : public DependencyParserModelTest {
 public:
  DependencyParserModelValidTest()
      : dependency_parser_model_(GetValidDependencyParserModel()) {}

 protected:
  std::unique_ptr<DependencyParserModel> dependency_parser_model_;
};

TEST_F(DependencyParserModelValidTest, ValidModelFileProvided) {
  histogram_tester_.ExpectUniqueSample(
      "Accessibility.DependencyParserModel.DependencyParserModelState",
      DependencyParserModelState::kModelAvailable, 1);
  histogram_tester_.ExpectTotalCount(
      "Accessibility.DependencyParserModel.InvalidModelFile", 0);
  histogram_tester_.ExpectTotalCount(
      "Accessibility.DependencyParserModel.Create.Duration", 1);
}

TEST_F(DependencyParserModelValidTest, GetDependencyHeads) {
  // The dependency tree for the input string:
  // # dessert(5)
  // #   children:
  // #     - cream(1)
  // #       children:
  // #         - ice(0)
  // #     - is(2)
  // #     - a(3)
  // #     - frozen(4)
  // #     - made(7)
  // #       children:
  // #         - typically(6)
  // #         - milk(9)
  // #           children:
  // #             - from(8)
  // #             - cream(11)
  // #               children:
  // #                 - or(10)
  const std::vector<std::string> input = {
      "Ice",       "cream", "is",   "a",    "frozen", "dessert",
      "typically", "made",  "from", "milk", "or",     "cream"};
  auto prediction = dependency_parser_model_->GetDependencyHeads(input);
  EXPECT_THAT(prediction,
              ::testing::ElementsAre(1, 5, 5, 5, 5, 5, 7, 5, 9, 7, 11, 9));

  histogram_tester_.ExpectUniqueSample(
      "Accessibility.DependencyParserModel.Inference.LengthInTokens",
      input.size(), 1);
  histogram_tester_.ExpectTotalCount(
      "Accessibility.DependencyParserModel.Inference.Duration", 1);
  histogram_tester_.ExpectUniqueSample(
      "Accessibility.DependencyParserModel.Inference.Succeed", true, 1);
}

TEST_F(DependencyParserModelValidTest, GetPhraseBoundaries) {
  EXPECT_THAT(dependency_parser_model_->GetPhraseBoundaries(
                  u"Never feel heavy or earthbound, no worries or doubts "
                  u"interfere."),
              ::testing::ElementsAre(0, 32));
  EXPECT_THAT(dependency_parser_model_->GetPhraseBoundaries(u""),
              ::testing::IsEmpty());
}

TEST_F(DependencyParserModelValidTest, GetPhraseBoundariesSingleToken) {
  EXPECT_THAT(dependency_parser_model_->GetPhraseBoundaries(u"Hello"),
              ::testing::ElementsAre(0));
  // A single token shouldn't need to run model inference.
  histogram_tester_.ExpectTotalCount(
      "Accessibility.DependencyParserModel.Inference.Duration", 0);
}

TEST_F(DependencyParserModelValidTest, GetPhraseBoundariesSafetyBounds) {
  // Sentences that are too long are skipped.
  const std::u16string too_long(1001, u'a');
  EXPECT_THAT(dependency_parser_model_->GetPhraseBoundaries(too_long),
              ::testing::IsEmpty());

  // Sentences with too many tokens are skipped, even if they're under the
  // length limit.
  std::u16string too_many_tokens;
  for (int i = 0; i < 201; i++) {
    too_many_tokens += u"a ";
  }
  EXPECT_THAT(dependency_parser_model_->GetPhraseBoundaries(too_many_tokens),
              ::testing::IsEmpty());

  histogram_tester_.ExpectTotalCount(
      "Accessibility.DependencyParserModel.Inference.Duration", 0);
}

TEST_F(DependencyParserModelValidTest, GetPhraseBoundariesForSentences) {
  EXPECT_THAT(
      dependency_parser_model_->GetPhraseBoundariesForSentences(
          {u"Hello", u"",
           u"Never feel heavy or earthbound, no worries or doubts "
           u"interfere."}),
      ::testing::ElementsAre(::testing::ElementsAre(0), ::testing::IsEmpty(),
                             ::testing::ElementsAre(0, 32)));
}

}  // namespace
