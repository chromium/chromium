// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/lobster/image_fetcher.h"

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/ash/lobster/mock/mock_snapper_provider.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace {

constexpr int kPreviewImageDimensionSize = 512;

// Forked from c/b/a/wallpaper_handlers/sea_pen_fetcher_unittest.cc
const SkBitmap CreateTestBitmap() {
  return gfx::test::CreateBitmap(kPreviewImageDimensionSize, SK_ColorMAGENTA);
}

const std::string_view GetTestJpgBytes() {
  static const base::NoDestructor<std::string> jpg_bytes([] {
    SkBitmap bitmap = CreateTestBitmap();
    std::vector<unsigned char> data;
    gfx::JPEGCodec::Encode(bitmap, /*quality=*/50, &data);
    return std::string(data.begin(), data.end());
  }());
  return *jpg_bytes;
}

std::unique_ptr<manta::proto::Response> CreateFakeMantaResponse(
    size_t num_candidates) {
  auto response = std::make_unique<manta::proto::Response>();
  for (size_t i = 0; i < num_candidates; ++i) {
    auto* output_data = response->add_output_data();
    output_data->mutable_image()->set_serialized_bytes(
        std::string(GetTestJpgBytes()));
  }
  return response;
}

bool AreJpgBytesClose(const SkBitmap& expected_bitmap,
                      const std::string& actual_image_bytes) {
  std::unique_ptr<SkBitmap> actual_bitmap = gfx::JPEGCodec::Decode(
      reinterpret_cast<const unsigned char*>(actual_image_bytes.data()),
      actual_image_bytes.size());
  // Use `AreBitmapsClose` because JPG encoding/decoding can alter the color
  // slightly.
  return actual_bitmap != nullptr &&
         gfx::test::AreBitmapsClose(expected_bitmap, *actual_bitmap,
                                    /*max_deviation=*/1);
}

class ImageFetcherTest : public testing::Test {
 public:
  ImageFetcherTest() {}
  ImageFetcherTest(const ImageFetcherTest&) = delete;
  ImageFetcherTest& operator=(const ImageFetcherTest&) = delete;

  ~ImageFetcherTest() override = default;

 private:
  base::test::TaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(ImageFetcherTest, RequestCandidatesCallSnapperProvider) {
  MockSnapperProvider snapper_provider;
  ImageFetcher image_fetcher(&snapper_provider);

  manta::proto::Request expected;
  manta::proto::RequestConfig& request_config =
      *expected.mutable_request_config();
  manta::proto::ImageDimensions& image_dimensions =
      *request_config.mutable_image_dimensions();
  manta::proto::InputData& input_data = *expected.add_input_data();

  expected.set_feature_name(manta::proto::FeatureName::CHROMEOS_LOBSTER);
  request_config.set_num_outputs(2);
  image_dimensions.set_width(kPreviewImageDimensionSize);
  image_dimensions.set_height(kPreviewImageDimensionSize);
  input_data.set_text("a lovely cake");

  EXPECT_CALL(snapper_provider,
              Call(base::test::EqualsProto(expected), testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [](const manta::proto::Request& request,
             net::NetworkTrafficAnnotationTag traffic_annotation,
             manta::MantaProtoResponseCallback done_callback) {
            std::move(done_callback)
                .Run(CreateFakeMantaResponse(2),
                     {.status_code = manta::MantaStatusCode::kOk,
                      .message = ""});
          }));

  base::test::TestFuture<const std::vector<ash::LobsterImageCandidate>&> future;

  image_fetcher.RequestPreviewCandidates("a lovely cake", 2,
                                         future.GetCallback());

  const std::vector<ash::LobsterImageCandidate>& actual_image_candidates =
      future.Get();
  EXPECT_EQ(actual_image_candidates.size(), 2u);
  EXPECT_TRUE(AreJpgBytesClose(CreateTestBitmap(),
                               actual_image_candidates[0].image_bytes));
  EXPECT_TRUE(AreJpgBytesClose(CreateTestBitmap(),
                               actual_image_candidates[1].image_bytes));
}

TEST_F(ImageFetcherTest,
       RequestCandidatesReturnsEmptyResponseIfMantaProviderHasAGenericError) {
  MockSnapperProvider snapper_provider;
  ImageFetcher image_fetcher(&snapper_provider);

  manta::proto::Request expected;
  manta::proto::RequestConfig& request_config =
      *expected.mutable_request_config();
  manta::proto::ImageDimensions& image_dimensions =
      *request_config.mutable_image_dimensions();
  manta::proto::InputData& input_data = *expected.add_input_data();

  expected.set_feature_name(manta::proto::FeatureName::CHROMEOS_LOBSTER);
  request_config.set_num_outputs(2);
  image_dimensions.set_width(kPreviewImageDimensionSize);
  image_dimensions.set_height(kPreviewImageDimensionSize);
  input_data.set_text("a sweet candy");

  EXPECT_CALL(snapper_provider,
              Call(base::test::EqualsProto(expected), testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [](const manta::proto::Request& request,
             net::NetworkTrafficAnnotationTag traffic_annotation,
             manta::MantaProtoResponseCallback done_callback) {
            std::move(done_callback)
                .Run(std::make_unique<manta::proto::Response>(),
                     {.status_code = manta::MantaStatusCode::kGenericError,
                      .message = "generic error"});
          }));

  base::test::TestFuture<const std::vector<ash::LobsterImageCandidate>&> future;

  image_fetcher.RequestPreviewCandidates("a sweet candy", 2,
                                         future.GetCallback());

  EXPECT_EQ(future.Get().size(), 0u);
}

}  // namespace
