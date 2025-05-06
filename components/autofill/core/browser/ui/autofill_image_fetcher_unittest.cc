// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/autofill_image_fetcher.h"

#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/ui/autofill_image_fetcher_base.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/image_fetcher/core/mock_image_fetcher.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/resources/grit/ui_resources.h"

using base::Bucket;
using testing::_;

namespace autofill {

class AutofillImageFetcherForTest : public AutofillImageFetcher {
 public:
  AutofillImageFetcherForTest()
      : mock_image_fetcher_(
            std::make_unique<image_fetcher::MockImageFetcher>()) {}
  ~AutofillImageFetcherForTest() override = default;

  image_fetcher::MockImageFetcher* mock_image_fetcher() const {
    return mock_image_fetcher_.get();
  }

  void set_card_art_image_override(const gfx::Image& card_art_image_override) {
    card_art_image_override_ = card_art_image_override;
  }

  void SimulateOnImageFetched(const GURL& url,
                              const gfx::Image& image,
                              ImageType image_type) {
    OnImageFetched(url, image_type, image, image_fetcher::RequestMetadata());
  }

  // AutofillImageFetcher:
  image_fetcher::ImageFetcher* GetImageFetcher() override {
    return mock_image_fetcher_.get();
  }

  base::WeakPtr<AutofillImageFetcher> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  GURL ResolveImageURL(const GURL& image_url,
                       ImageType image_type) const override {
    if (image_url.spec() == kCapitalOneCardArtUrl) {
      return image_url;
    }

    // A FIFE image fetching param suffix is appended to the URL. The image
    // should be center cropped and of Size(32, 20).
    return GURL(image_url.spec() + "=w32-h20-n");
  }

  gfx::Image ResolveCardArtImage(const GURL& card_art_url,
                                 const gfx::Image& card_art_image) override {
    return !card_art_image_override_.IsEmpty() ? card_art_image_override_
                                               : card_art_image;
  }

 private:
  gfx::Image card_art_image_override_;

  std::unique_ptr<image_fetcher::MockImageFetcher> mock_image_fetcher_;
  base::WeakPtrFactory<AutofillImageFetcherForTest> weak_ptr_factory_{this};
};

class AutofillImageFetcherTest : public testing::Test {
 public:
  AutofillImageFetcherTest()
      : autofill_image_fetcher_(
            std::make_unique<AutofillImageFetcherForTest>()) {}

  gfx::Image& GetTestImage(int resource_id) {
    return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
        resource_id);
  }

  image_fetcher::MockImageFetcher* mock_image_fetcher() {
    return autofill_image_fetcher_->mock_image_fetcher();
  }

  AutofillImageFetcherForTest* autofill_image_fetcher() {
    return autofill_image_fetcher_.get();
  }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<AutofillImageFetcherForTest> autofill_image_fetcher_;
};

TEST_F(AutofillImageFetcherTest, FetchCreditCardArtImagesForURLs_Success) {
  // The credit card network images cannot be found in the tests, but it should
  // be okay since we don't care what the images are.
  gfx::Image fake_image1 = GetTestImage(IDR_DEFAULT_FAVICON);
  gfx::Image fake_image2 = GetTestImage(IDR_DEFAULT_FAVICON);
  GURL fake_url1 = GURL("https://www.example.com/fake_image1");
  GURL fake_url2 = GURL(kCapitalOneCardArtUrl);
  base::HistogramTester histogram_tester;

  // Expect to be called twice. The 'normal' URL should have a size appended to
  // it, whilst the capitalone image is 'special' and does not.
  EXPECT_CALL(
      *mock_image_fetcher(),
      FetchImageAndData_(GURL(fake_url1.spec() + "=w32-h20-n"), _, _, _));
  EXPECT_CALL(*mock_image_fetcher(), FetchImageAndData_(fake_url2, _, _, _));
  std::vector<GURL> urls = {fake_url1, fake_url2};
  autofill_image_fetcher()->FetchCreditCardArtImagesForURLs(
      urls, base::span_from_ref(AutofillImageFetcherBase::ImageSize::kSmall));

  // Simulate successful image fetching (for image with URL) -> expect the
  // callback to be called.
  autofill_image_fetcher()->SimulateOnImageFetched(
      fake_url1, fake_image1,
      AutofillImageFetcherBase::ImageType::kCreditCardArtImage);
  autofill_image_fetcher()->SimulateOnImageFetched(
      fake_url2, fake_image2,
      AutofillImageFetcherBase::ImageType::kCreditCardArtImage);

  EXPECT_TRUE(gfx::test::AreImagesEqual(
      fake_image1,
      *autofill_image_fetcher()->GetCachedImageForUrl(
          fake_url1,
          AutofillImageFetcherBase::ImageType::kCreditCardArtImage)));
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      fake_image2,
      *autofill_image_fetcher()->GetCachedImageForUrl(
          fake_url2,
          AutofillImageFetcherBase::ImageType::kCreditCardArtImage)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.ImageFetcher.CreditCardArt.Result"),
              BucketsAre(Bucket(false, 0), Bucket(true, 2)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.ImageFetcher.CreditCardArt.OverallResultOnBrowserStart"),
      BucketsAre(Bucket(false, 0), Bucket(true, 2)));
}

TEST_F(AutofillImageFetcherTest, FetchImage_ResolveImage) {
  // Set the AutofillImageFetcher to replace the input `fake_image1` in
  // ResolveImage.
  gfx::Image override_image = gfx::test::CreateImage(5, 5);
  autofill_image_fetcher()->set_card_art_image_override(override_image);

  GURL fake_url1 = GURL("https://www.example.com/fake_image1");
  gfx::Image fake_image1 = gfx::test::CreateImage(1, 2);

  autofill_image_fetcher()->SimulateOnImageFetched(
      fake_url1, fake_image1,
      AutofillImageFetcherBase::ImageType::kCreditCardArtImage);

  // The received image should be `override_image`, because ResolveImage should
  // have changed it.
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      override_image,
      *autofill_image_fetcher()->GetCachedImageForUrl(
          fake_url1,
          AutofillImageFetcherBase::ImageType::kCreditCardArtImage)));
}

TEST_F(AutofillImageFetcherTest,
       FetchCreditCardArtImagesForURLs_Failure_RetryFailure) {
  GURL fake_url = GURL("https://www.example.com/fake_image1");
  std::vector<GURL> urls = {fake_url};
  base::HistogramTester histogram_tester;

  // Expect to be called once.
  EXPECT_CALL(*mock_image_fetcher(), FetchImageAndData_);

  autofill_image_fetcher()->FetchCreditCardArtImagesForURLs(
      urls, base::span_from_ref(AutofillImageFetcherBase::ImageSize::kSmall));
  // Simulate image fetch failure.
  autofill_image_fetcher()->SimulateOnImageFetched(
      fake_url, gfx::Image(),
      AutofillImageFetcherBase::ImageType::kCreditCardArtImage);

  // Empty images are not cached, so the result should be a `nullptr`.
  EXPECT_FALSE(autofill_image_fetcher()->GetCachedImageForUrl(
      fake_url, AutofillImageFetcherBase::ImageType::kCreditCardArtImage));
  // Verify one failure logged.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.ImageFetcher.CreditCardArt.Result"),
              BucketsAre(Bucket(false, 1), Bucket(true, 0)));
  // Verify overall histogram is not logged yet.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.ImageFetcher.CreditCardArt.OverallResultOnBrowserStart"),
      BucketsAre(Bucket(false, 0), Bucket(true, 0)));

  // Expect the second fetch attempt after the delay.
  EXPECT_CALL(*mock_image_fetcher(), FetchImageAndData_);

  // Fast forward time to trigger the retry.
  task_environment().FastForwardBy(base::Minutes(2));
  // Simulate image fetch failure again.
  autofill_image_fetcher()->SimulateOnImageFetched(
      fake_url, gfx::Image(),
      AutofillImageFetcherBase::ImageType::kCreditCardArtImage);

  // Verify the image is still not cached.
  EXPECT_FALSE(autofill_image_fetcher()->GetCachedImageForUrl(
      fake_url, AutofillImageFetcherBase::ImageType::kCreditCardArtImage));
  // Verify two failures logged.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.ImageFetcher.CreditCardArt.Result"),
              BucketsAre(Bucket(false, 2), Bucket(true, 0)));
  // Verify a single overall failure logged.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.ImageFetcher.CreditCardArt.OverallResultOnBrowserStart"),
      BucketsAre(Bucket(false, 1), Bucket(true, 0)));

  // Verify a maximum of 2 attempts are made. Fast-forward time to verify this.
  EXPECT_CALL(*mock_image_fetcher(), FetchImageAndData_).Times(0);

  task_environment().FastForwardBy(base::Minutes(2));
}

TEST_F(AutofillImageFetcherTest,
       FetchCreditCardArtImagesForURLs_Failure_RetrySuccess) {
  GURL fake_url = GURL("https://www.example.com/fake_image1");
  gfx::Image fake_image = GetTestImage(IDR_DEFAULT_FAVICON);
  std::vector<GURL> urls = {fake_url};
  base::HistogramTester histogram_tester;

  // Expect to be called once.
  EXPECT_CALL(*mock_image_fetcher(), FetchImageAndData_);

  autofill_image_fetcher()->FetchCreditCardArtImagesForURLs(
      urls, base::span_from_ref(AutofillImageFetcherBase::ImageSize::kSmall));
  // Simulate image fetch failure.
  autofill_image_fetcher()->SimulateOnImageFetched(
      fake_url, gfx::Image(),
      AutofillImageFetcherBase::ImageType::kCreditCardArtImage);

  // Empty images are not cached, so the result should be a `nullptr`.
  EXPECT_FALSE(autofill_image_fetcher()->GetCachedImageForUrl(
      fake_url, AutofillImageFetcherBase::ImageType::kCreditCardArtImage));
  // Verify one failure logged.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.ImageFetcher.CreditCardArt.Result"),
              BucketsAre(Bucket(false, 1), Bucket(true, 0)));
  // Verify the overall histogram is not logged yet.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.ImageFetcher.CreditCardArt.OverallResultOnBrowserStart"),
      BucketsAre(Bucket(false, 0), Bucket(true, 0)));

  // Expect the second fetch attempt after the delay.
  EXPECT_CALL(*mock_image_fetcher(), FetchImageAndData_);

  // Fast forward time to trigger the retry.
  task_environment().FastForwardBy(base::Minutes(2));
  // Simulate image fetch success.
  autofill_image_fetcher()->SimulateOnImageFetched(
      fake_url, fake_image,
      AutofillImageFetcherBase::ImageType::kCreditCardArtImage);

  // Verify the image is cached.
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      fake_image,
      *autofill_image_fetcher()->GetCachedImageForUrl(
          fake_url, AutofillImageFetcherBase::ImageType::kCreditCardArtImage)));
  // Verify one failure and one success logged.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.ImageFetcher.CreditCardArt.Result"),
              BucketsAre(Bucket(false, 1), Bucket(true, 1)));
  // Verify a single overall success logged.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.ImageFetcher.CreditCardArt.OverallResultOnBrowserStart"),
      BucketsAre(Bucket(false, 0), Bucket(true, 1)));
}

TEST_F(AutofillImageFetcherTest,
       FetchCreditCardArtImagesForURLs_Failure_RetryDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kAutofillRetryImageFetchOnFailure});

  GURL fake_url = GURL("https://www.example.com/fake_image1");
  std::vector<GURL> urls = {fake_url};
  base::HistogramTester histogram_tester;

  // Expect to be called once.
  EXPECT_CALL(*mock_image_fetcher(), FetchImageAndData_);

  autofill_image_fetcher()->FetchCreditCardArtImagesForURLs(
      urls, base::span_from_ref(AutofillImageFetcherBase::ImageSize::kSmall));
  // Simulate image fetch failure.
  autofill_image_fetcher()->SimulateOnImageFetched(
      fake_url, gfx::Image(),
      AutofillImageFetcherBase::ImageType::kCreditCardArtImage);

  // Empty images are not cached, so the result should be a `nullptr`.
  EXPECT_FALSE(autofill_image_fetcher()->GetCachedImageForUrl(
      fake_url, AutofillImageFetcherBase::ImageType::kCreditCardArtImage));
  // Verify one failure logged.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.ImageFetcher.CreditCardArt.Result"),
              BucketsAre(Bucket(false, 1), Bucket(true, 0)));
  // Verify the overall histogram is not logged.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.ImageFetcher.CreditCardArt.OverallResultOnBrowserStart"),
      BucketsAre(Bucket(false, 0), Bucket(true, 0)));

  // Verify no more fetch attempts since retry is disabled.
  EXPECT_CALL(*mock_image_fetcher(), FetchImageAndData_).Times(0);

  // Fast forward time to trigger the retry.
  task_environment().FastForwardBy(base::Minutes(2));
}

TEST_F(AutofillImageFetcherTest, FetchValuableImagesForURLs_Success) {
  gfx::Image fake_image = GetTestImage(IDR_DEFAULT_FAVICON);
  GURL fake_url = GURL("https://www.example.com/fake_image");
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*mock_image_fetcher(), FetchImageAndData_).Times(1);
  autofill_image_fetcher()->FetchValuableImagesForURLs({fake_url});

  autofill_image_fetcher()->SimulateOnImageFetched(
      fake_url, fake_image,
      AutofillImageFetcherBase::ImageType::kValuableImage);
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      fake_image,
      *autofill_image_fetcher()->GetCachedImageForUrl(
          fake_url, AutofillImageFetcherBase::ImageType::kValuableImage)));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.ImageFetcher.ValuableImage.Result"),
              BucketsAre(Bucket(false, 0), Bucket(true, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.ImageFetcher.ValuableImage.OverallResultOnBrowserStart"),
      BucketsAre(Bucket(false, 0), Bucket(true, 1)));
}

TEST_F(AutofillImageFetcherTest,
       FetchValuableImagesForURLs_Failure_RetryFailure) {
  gfx::Image fake_image;
  GURL fake_url = GURL("https://www.example.com/fake_image");
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*mock_image_fetcher(), FetchImageAndData_).Times(2);
  autofill_image_fetcher()->FetchValuableImagesForURLs({fake_url});

  autofill_image_fetcher()->SimulateOnImageFetched(
      fake_url, fake_image,
      AutofillImageFetcherBase::ImageType::kValuableImage);
  // Empty images are not cached, so the result should be a `nullptr`.
  EXPECT_FALSE(autofill_image_fetcher()->GetCachedImageForUrl(
      fake_url, AutofillImageFetcherBase::ImageType::kValuableImage));
  // Verify one failure is logged.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.ImageFetcher.ValuableImage.Result"),
              BucketsAre(Bucket(false, 1), Bucket(true, 0)));
  // Verify overall histogram is not logged yet.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.ImageFetcher.ValuableImage.OverallResultOnBrowserStart"),
      BucketsAre(Bucket(false, 0), Bucket(true, 0)));

  // Fast forward time to trigger the retry.
  task_environment().FastForwardBy(base::Minutes(2));

  autofill_image_fetcher()->SimulateOnImageFetched(
      fake_url, fake_image,
      AutofillImageFetcherBase::ImageType::kValuableImage);
  // Empty images are not cached, so the result should be a `nullptr`.
  EXPECT_FALSE(autofill_image_fetcher()->GetCachedImageForUrl(
      fake_url, AutofillImageFetcherBase::ImageType::kValuableImage));
  // Verify second failure is logged.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.ImageFetcher.ValuableImage.Result"),
              BucketsAre(Bucket(false, 2), Bucket(true, 0)));
  // Verify overall histogram is logged now.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.ImageFetcher.ValuableImage.OverallResultOnBrowserStart"),
      BucketsAre(Bucket(false, 1), Bucket(true, 0)));

  // Check that no more retries are conducted.
  task_environment().FastForwardBy(base::Minutes(2));
}
}  // namespace autofill
