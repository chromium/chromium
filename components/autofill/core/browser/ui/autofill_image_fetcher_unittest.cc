// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/autofill_image_fetcher.h"

#include "base/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/image_fetcher/core/mock_image_fetcher.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/resources/grit/ui_resources.h"

using testing::_;

namespace autofill {

class TestAutofillImageFetcher : public AutofillImageFetcher {
 public:
  explicit TestAutofillImageFetcher(
      std::unique_ptr<image_fetcher::MockImageFetcher> image_fetcher)
      : AutofillImageFetcher(nullptr, nullptr) {
    image_fetcher_ = std::move(image_fetcher);
  }

  ~TestAutofillImageFetcher() override = default;

  void FetchImageForUrl(const scoped_refptr<ImageFetchOperation>& operation,
                        const std::string& card_server_id,
                        const GURL& card_art_url) override {
    AutofillImageFetcher::FetchImageForUrl(operation, card_server_id,
                                           card_art_url);
    current_operation_ = operation;
  }

  const scoped_refptr<ImageFetchOperation>& current_operation() {
    return current_operation_;
  }

 private:
  scoped_refptr<ImageFetchOperation> current_operation_;
};

class AutofillImageFetcherTest : public testing::Test {
 public:
  void SetUp() override {
    auto image_fetcher = std::make_unique<image_fetcher::MockImageFetcher>();
    image_fetcher_ = image_fetcher.get();
    autofill_image_fetcher_ =
        std::make_unique<TestAutofillImageFetcher>(std::move(image_fetcher));
  }

  void SimulateOnImageFetched(const std::string& server_id,
                              const gfx::Image& image) {
    TestAutofillImageFetcher::OnCardArtImageFetched(
        autofill_image_fetcher()->current_operation(), server_id, image,
        image_fetcher::RequestMetadata());
  }

  void ValidateResult(std::map<std::string, gfx::Image> received_images,
                      std::map<std::string, gfx::Image> expected_images) {
    ASSERT_EQ(expected_images.size(), received_images.size());
    for (const auto& expected_pair : expected_images) {
      const gfx::Image& expected_image = expected_pair.second;
      const gfx::Image& received_image = received_images[expected_pair.first];
      EXPECT_TRUE(gfx::test::AreImagesEqual(expected_image, received_image));
    }
  }

  image_fetcher::MockImageFetcher* mock_image_fetcher() {
    return image_fetcher_;
  }

  TestAutofillImageFetcher* autofill_image_fetcher() {
    return autofill_image_fetcher_.get();
  }

 private:
  std::unique_ptr<TestAutofillImageFetcher> autofill_image_fetcher_;
  image_fetcher::MockImageFetcher* image_fetcher_;
};

TEST_F(AutofillImageFetcherTest, FetchImage_Success) {
  // The credit card network images cannot be found in the tests, but it should
  // be okay since we don't care what the images are.
  gfx::Image fake_image1 =
      ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
          IDR_DEFAULT_FAVICON);
  gfx::Image fake_image2 =
      ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
          IDR_DEFAULT_FAVICON_DARK);
  std::map<std::string, gfx::Image> expected_images = {
      {"server_id1", fake_image1}, {"server_id2", fake_image2}};

  // Expect callback to be called with some received images.
  std::map<std::string, gfx::Image> received_images;
  auto callback = base::BindLambdaForTesting(
      [&](const std::map<std::string, gfx::Image>& card_art_image_map) {
        received_images = card_art_image_map;
      });

  base::HistogramTester histogram_tester;
  // Expect to be called twice.
  EXPECT_CALL(*mock_image_fetcher(), FetchImageAndData_(_, _, _, _)).Times(2);
  std::map<std::string, GURL> url_map = {
      {"server_id1", GURL("http://www.example.com/fake_image1")},
      {"server_id2", GURL("http://www.example.com/fake_image2")}};
  autofill_image_fetcher()->FetchImagesForUrls(url_map, callback);

  // Simulate successful image fetching (for image with URL) -> expect the
  // callback to be called.
  SimulateOnImageFetched("server_id1", fake_image1);
  SimulateOnImageFetched("server_id2", fake_image2);

  ValidateResult(received_images, expected_images);
  histogram_tester.ExpectBucketCount("Autofill.ImageFetcher.Result", true, 2);
}

TEST_F(AutofillImageFetcherTest, FetchImage_InvalidUrlFailure) {
  gfx::Image fake_image1 =
      ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
          IDR_DEFAULT_FAVICON);
  std::map<std::string, gfx::Image> expected_images = {
      {"server_id1", fake_image1}};

  // Expect callback to be called with expected images.
  std::map<std::string, gfx::Image> received_images;
  auto callback = base::BindLambdaForTesting(
      [&](const std::map<std::string, gfx::Image>& card_art_image_map) {
        received_images = card_art_image_map;
      });

  base::HistogramTester histogram_tester;
  // Expect to be called once with one invalid url.
  EXPECT_CALL(*mock_image_fetcher(), FetchImageAndData_(_, _, _, _)).Times(1);
  std::map<std::string, GURL> url_map = {
      {"server_id1", GURL("http://www.example.com/fake_image1")},
      {"server_id2", GURL("")}};
  autofill_image_fetcher()->FetchImagesForUrls(url_map, callback);

  // Simulate successful image fetching (for image with URL) -> expect the
  // callback to be called.
  SimulateOnImageFetched("server_id1", fake_image1);

  ValidateResult(received_images, expected_images);
  histogram_tester.ExpectBucketCount("Autofill.ImageFetcher.Result", true, 1);
  histogram_tester.ExpectBucketCount("Autofill.ImageFetcher.Result", false, 1);
}

TEST_F(AutofillImageFetcherTest, FetchImage_ServerFailure) {
  std::map<std::string, gfx::Image> expected_images = {};

  // Expect callback to be called with some received images.
  std::map<std::string, gfx::Image> received_images;
  auto callback = base::BindLambdaForTesting(
      [&](const std::map<std::string, gfx::Image>& card_art_image_map) {
        received_images = card_art_image_map;
      });

  base::HistogramTester histogram_tester;
  // Expect to be called once.
  EXPECT_CALL(*mock_image_fetcher(), FetchImageAndData_(_, _, _, _)).Times(1);
  std::map<std::string, GURL> url_map = {
      {"server_id1", GURL("http://www.example.com/fake_image1")}};
  autofill_image_fetcher()->FetchImagesForUrls(url_map, callback);

  // Simulate failed image fetching (for image with URL) -> expect the
  // callback to be called.
  SimulateOnImageFetched("server_id1", gfx::Image());

  ValidateResult(received_images, expected_images);
  histogram_tester.ExpectBucketCount("Autofill.ImageFetcher.Result", false, 1);
}

}  // namespace autofill
