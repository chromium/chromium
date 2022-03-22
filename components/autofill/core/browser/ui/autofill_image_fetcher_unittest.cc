// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/autofill_image_fetcher.h"

#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_art_image.h"
#include "components/autofill/core/browser/test_autofill_tick_clock.h"
#include "components/autofill/core/common/autofill_tick_clock.h"
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
                        const GURL& card_art_url) override {
    AutofillImageFetcher::FetchImageForUrl(operation, card_art_url);
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

  void SimulateOnImageFetched(
      const GURL& url,
      const absl::optional<base::TimeTicks>& fetch_image_request_timestamp,
      const gfx::Image& image) {
    TestAutofillImageFetcher::OnCardArtImageFetched(
        autofill_image_fetcher()->current_operation(), url,
        fetch_image_request_timestamp, image, image_fetcher::RequestMetadata());
  }

  void ValidateResult(std::map<GURL, gfx::Image> received_images,
                      std::map<GURL, gfx::Image> expected_images) {
    ASSERT_EQ(expected_images.size(), received_images.size());
    for (const auto& [expected_url, expected_image] : expected_images) {
      const gfx::Image& received_image = received_images[expected_url];
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
  raw_ptr<image_fetcher::MockImageFetcher> image_fetcher_;
};

TEST_F(AutofillImageFetcherTest, FetchImage_Success) {
  base::TimeTicks now = AutofillTickClock::NowTicks();
  TestAutofillTickClock test_clock;
  test_clock.SetNowTicks(now);

  // The credit card network images cannot be found in the tests, but it should
  // be okay since we don't care what the images are.
  gfx::Image fake_image1 =
      ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
          IDR_DEFAULT_FAVICON);
  gfx::Image fake_image2 =
      ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
          IDR_DEFAULT_FAVICON_DARK);
  GURL fake_url1 = GURL("http://www.example.com/fake_image1");
  GURL fake_url2 = GURL("http://www.example.com/fake_image2");

  std::map<GURL, gfx::Image> expected_images = {{fake_url1, fake_image1},
                                                {fake_url2, fake_image2}};

  // Expect callback to be called with some received images.
  std::map<GURL, gfx::Image> received_images;
  auto callback = base::BindLambdaForTesting(
      [&](std::vector<std::unique_ptr<CreditCardArtImage>> card_art_images) {
        for (auto& entry : card_art_images)
          received_images[entry->card_art_url] = entry->card_art_image;
      });

  base::HistogramTester histogram_tester;
  // Expect to be called twice.
  EXPECT_CALL(*mock_image_fetcher(), FetchImageAndData_(_, _, _, _)).Times(2);
  std::vector<GURL> urls = {fake_url1, fake_url2};
  autofill_image_fetcher()->FetchImagesForUrls(urls, callback);

  // Advance the time to make the latency values more realistic.
  test_clock.Advance(base::Microseconds(2000));
  // Simulate successful image fetching (for image with URL) -> expect the
  // callback to be called.
  SimulateOnImageFetched(fake_url1, now, fake_image1);
  SimulateOnImageFetched(fake_url2, now, fake_image2);

  ValidateResult(std::move(received_images), expected_images);
  histogram_tester.ExpectBucketCount("Autofill.ImageFetcher.Result", true, 2);
  histogram_tester.ExpectTotalCount("Autofill.ImageFetcher.RequestLatency", 2);
  histogram_tester.ExpectUniqueSample("Autofill.ImageFetcher.RequestLatency", 2,
                                      2);
}

TEST_F(AutofillImageFetcherTest, FetchImage_InvalidUrlFailure) {
  base::TimeTicks now = AutofillTickClock::NowTicks();
  TestAutofillTickClock test_clock;
  test_clock.SetNowTicks(now);

  gfx::Image fake_image1 =
      ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
          IDR_DEFAULT_FAVICON);
  GURL fake_url1 = GURL("http://www.example.com/fake_image1");
  std::map<GURL, gfx::Image> expected_images = {{fake_url1, fake_image1}};

  std::map<GURL, gfx::Image> received_images;
  auto callback = base::BindLambdaForTesting(
      [&](std::vector<std::unique_ptr<CreditCardArtImage>> card_art_images) {
        for (auto& entry : card_art_images)
          received_images[entry->card_art_url] = entry->card_art_image;
      });

  base::HistogramTester histogram_tester;
  // Expect to be called once with one invalid url.
  EXPECT_CALL(*mock_image_fetcher(), FetchImageAndData_(_, _, _, _)).Times(1);
  std::vector<GURL> urls = {fake_url1, GURL("")};
  autofill_image_fetcher()->FetchImagesForUrls(urls, callback);

  test_clock.Advance(base::Microseconds(2000));
  // Simulate successful image fetching (for image with URL) -> expect the
  // callback to be called.
  SimulateOnImageFetched(fake_url1, now, fake_image1);

  ValidateResult(std::move(received_images), expected_images);
  histogram_tester.ExpectBucketCount("Autofill.ImageFetcher.Result", true, 1);
  histogram_tester.ExpectBucketCount("Autofill.ImageFetcher.Result", false, 1);
  histogram_tester.ExpectTotalCount("Autofill.ImageFetcher.RequestLatency", 1);
  histogram_tester.ExpectUniqueSample("Autofill.ImageFetcher.RequestLatency", 2,
                                      1);
}

TEST_F(AutofillImageFetcherTest, FetchImage_ServerFailure) {
  base::TimeTicks now = AutofillTickClock::NowTicks();
  TestAutofillTickClock test_clock;
  test_clock.SetNowTicks(now);

  GURL fake_url1 = GURL("http://www.example.com/fake_image1");
  std::map<GURL, gfx::Image> expected_images;

  // Expect callback to be called with some received images.
  std::map<GURL, gfx::Image> received_images;
  auto callback = base::BindLambdaForTesting(
      [&](std::vector<std::unique_ptr<CreditCardArtImage>> card_art_images) {
        for (auto& entry : card_art_images)
          received_images[entry->card_art_url] = entry->card_art_image;
      });

  base::HistogramTester histogram_tester;
  // Expect to be called once.
  EXPECT_CALL(*mock_image_fetcher(), FetchImageAndData_(_, _, _, _)).Times(1);
  std::vector<GURL> urls = {fake_url1};
  autofill_image_fetcher()->FetchImagesForUrls(urls, callback);

  test_clock.Advance(base::Microseconds(2000));
  // Simulate failed image fetching (for image with URL) -> expect the
  // callback to be called.
  SimulateOnImageFetched(fake_url1, now, gfx::Image());

  ValidateResult(std::move(received_images), expected_images);
  histogram_tester.ExpectBucketCount("Autofill.ImageFetcher.Result", false, 1);
  histogram_tester.ExpectTotalCount("Autofill.ImageFetcher.RequestLatency", 1);
  histogram_tester.ExpectUniqueSample("Autofill.ImageFetcher.RequestLatency", 2,
                                      1);
}

}  // namespace autofill
