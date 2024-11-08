// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/mantis/media_app/mantis_media_app_untrusted_processor.h"

#include <cstdint>
#include <memory>
#include <variant>

#include "base/functional/overloaded.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/mantis/mojom/mantis_processor.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::base::test::RunOnceCallback;
using ::base::test::TestFuture;
using ::mantis::mojom::MantisError;
using ::mantis::mojom::MantisResult;
using ::mantis::mojom::MantisResultPtr;
using ::mantis::mojom::SafetyClassifierVerdict;

// Gets a random vector of number to represent a fake encoded image.
std::vector<uint8_t> GetFakeImage() {
  return {0x00, 0x7F, 0xFF, 0x10, 0x50, 0x90, 0x20, 0x60, 0xA0};
}

// Gets a random vector of number to represent a fake encoded mask.
std::vector<uint8_t> GetFakeMask() {
  return {0x10, 0x50, 0x90, 0x20, 0x60, 0xA0, 0x00, 0x7F, 0xFF};
}

// Gets a number for the seed.
uint32_t GetFakeSeed() {
  return 10;
}

// Gets a string for the prompt.
std::string GetFakePrompt() {
  return "a cute cat";
}

// Gets a random vector of number to represent a fake encoded result.
std::vector<uint8_t> GetFakeResult() {
  return {0x20, 0x23, 0x39, 0x30, 0x70, 0x30, 0xA0, 0x5B, 0xFA};
}

class MockMojoMantisProcessor : public mantis::mojom::MantisProcessor {
 public:
  explicit MockMojoMantisProcessor(
      mojo::PendingReceiver<mantis::mojom::MantisProcessor> processor)
      : receiver_(this, std::move(processor)) {}
  ~MockMojoMantisProcessor() override = default;

  MOCK_METHOD(void,
              Inpainting,
              (const std::vector<uint8_t>& image,
               const std::vector<uint8_t>& mask,
               uint32_t seed,
               InpaintingCallback callback),
              (override));
  MOCK_METHOD(void,
              GenerativeFill,
              (const std::vector<uint8_t>& image,
               const std::vector<uint8_t>& mask,
               uint32_t seed,
               const std::string& prompt,
               GenerativeFillCallback callback),
              (override));
  MOCK_METHOD(void,
              Segmentation,
              (const std::vector<uint8_t>& image,
               const std::vector<uint8_t>& prior,
               SegmentationCallback callback),
              (override));
  MOCK_METHOD(void,
              ClassifyImageSafety,
              (const std::vector<uint8_t>& image,
               ClassifyImageSafetyCallback callback),
              (override));

 private:
  mojo::Receiver<mantis::mojom::MantisProcessor> receiver_;
};

class UntrustedProcessorTest : public testing::Test {
 public:
  UntrustedProcessorTest()
      : processor_(mojo::NullReceiver()),
        mojo_mantis_processor_(processor_.GetReceiver()) {}

 protected:
  base::test::TaskEnvironment task_environment_;
  MantisMediaAppUntrustedProcessor processor_;
  MockMojoMantisProcessor mojo_mantis_processor_;
};

// MantisResultPtr is not copyable and unusable by parameterized tests.
using ImageInferenceTestCase = std::variant<MantisError, std::vector<uint8_t>>;

MantisResultPtr GetMantisResult(const ImageInferenceTestCase& test_case) {
  return std::visit(
      base::Overloaded{[](const MantisError& error) {
                         return MantisResult::NewError(error);
                       },
                       [](const std::vector<uint8_t>& result_image) {
                         return MantisResult::NewResultImage(result_image);
                       }},
      test_case);
}

class ImageInferenceTest
    : public UntrustedProcessorTest,
      public testing::WithParamInterface<ImageInferenceTestCase> {};

TEST_P(ImageInferenceTest, SegmentImage) {
  MantisResultPtr result = GetMantisResult(GetParam());

  EXPECT_CALL(mojo_mantis_processor_, Segmentation)
      .WillOnce(RunOnceCallback<2>(result.Clone()));

  base::test::TestFuture<mantis::mojom::MantisResultPtr> result_future;
  processor_.SegmentImage(GetFakeImage(), GetFakeMask(),
                          result_future.GetCallback());
  EXPECT_EQ(result_future.Take(), result);
}

TEST_P(ImageInferenceTest, GenerativeFillImage) {
  MantisResultPtr result = GetMantisResult(GetParam());

  EXPECT_CALL(mojo_mantis_processor_, GenerativeFill)
      .WillOnce(RunOnceCallback<4>(result.Clone()));

  base::test::TestFuture<mantis::mojom::MantisResultPtr> result_future;
  processor_.GenerativeFillImage(GetFakeImage(), GetFakeMask(), GetFakePrompt(),
                                 GetFakeSeed(), result_future.GetCallback());
  EXPECT_EQ(result_future.Take(), result);
}

TEST_P(ImageInferenceTest, InpaintImage) {
  MantisResultPtr result = GetMantisResult(GetParam());

  EXPECT_CALL(mojo_mantis_processor_, Inpainting)
      .WillOnce(RunOnceCallback<3>(result.Clone()));

  base::test::TestFuture<mantis::mojom::MantisResultPtr> result_future;
  processor_.InpaintImage(GetFakeImage(), GetFakeMask(), GetFakeSeed(),
                          result_future.GetCallback());
  EXPECT_EQ(result_future.Take(), result);
}

INSTANTIATE_TEST_SUITE_P(
    MantisMediaApp,
    ImageInferenceTest,
    testing::Values(MantisError::kUnknownError,
                    MantisError::kProcessorNotInitialized,
                    MantisError::kInputError,
                    MantisError::kProcessFailed,
                    MantisError::kMissingSegmenter,
                    GetFakeResult()),
    [](const testing::TestParamInfo<ImageInferenceTestCase>& info) {
      return std::visit(
          base::Overloaded{[](const MantisError& error) {
                             return testing::PrintToString(error);
                           },
                           [](const std::vector<uint8_t>& result_image) {
                             return std::string("kResultImage");
                           }},
          info.param);
    });

class ClassifyImageSafetyTest
    : public UntrustedProcessorTest,
      public testing::WithParamInterface<SafetyClassifierVerdict> {};

TEST_P(ClassifyImageSafetyTest, ClassifyImageSafety) {
  const SafetyClassifierVerdict& verdict = GetParam();

  EXPECT_CALL(mojo_mantis_processor_, ClassifyImageSafety)
      .WillOnce(RunOnceCallback<1>(verdict));

  base::test::TestFuture<mantis::mojom::SafetyClassifierVerdict> verdict_future;
  processor_.ClassifyImageSafety(GetFakeImage(), verdict_future.GetCallback());
  EXPECT_EQ(verdict_future.Take(), verdict);
}

INSTANTIATE_TEST_SUITE_P(MantisMediaApp,
                         ClassifyImageSafetyTest,
                         testing::Values(SafetyClassifierVerdict::kPass,
                                         SafetyClassifierVerdict::kFail),
                         testing::PrintToStringParamName());

}  // namespace
}  // namespace ash
