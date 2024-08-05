// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/lobster/image_fetcher.h"

#include "base/functional/callback.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/ash/lobster/mock/mock_snapper_provider.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr int kPreviewImageDimensionSize = 512;

class ImageFetcherTest : public testing::Test {
 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ImageFetcherTest, RequestCandidatesCallSnapperProvider) {
  MockSnapperProvider snapper_provider = MockSnapperProvider();
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
              Call(base::test::EqualsProto(expected), testing::_, testing::_));

  image_fetcher.RequestPreviewCandidates(
      "a lovely cake", 2,
      base::BindOnce([](const std::vector<ash::LobsterImageCandidate>&) {}));
}

}  // namespace
