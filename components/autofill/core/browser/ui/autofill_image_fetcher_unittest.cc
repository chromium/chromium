// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/autofill_image_fetcher.h"

#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/payments/constants.h"
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

  void set_card_art_url_override(const GURL& card_art_url_override) {
    card_art_url_override_ = card_art_url_override;
  }
  void set_card_art_image_override(const gfx::Image& card_art_image_override) {
    card_art_image_override_ = card_art_image_override;
  }

  void SimulateOnCardArtImageFetched(
      const GURL& url,
      const std::optional<base::TimeTicks>& fetch_image_request_timestamp,
      const gfx::Image& image) {
    OnCardArtImageFetched(url, fetch_image_request_timestamp, image,
                          image_fetcher::RequestMetadata());
  }

  // AutofillImageFetcher:
  image_fetcher::ImageFetcher* GetImageFetcher() override {
    return mock_image_fetcher_.get();
  }
  base::WeakPtr<AutofillImageFetcher> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }
  GURL ResolveCardArtURL(const GURL& card_art_url) override {
    return card_art_url_override_.is_valid()
               ? card_art_url_override_
               : AutofillImageFetcher::ResolveCardArtURL(card_art_url);
  }
  gfx::Image ResolveCardArtImage(const GURL& card_art_url,
                                 const gfx::Image& card_art_image) override {
    return !card_art_image_override_.IsEmpty()
               ? card_art_image_override_
               : AutofillImageFetcher::ResolveCardArtImage(card_art_url,
                                                           card_art_image);
  }

 private:
  GURL card_art_url_override_;
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

TEST_F(AutofillImageFetcherTest, FetchImage_Success) {
  base::TimeTicks now = base::TimeTicks::Now();

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

  // Advance the time to make the latency values more realistic.
  task_environment().FastForwardBy(base::Milliseconds(200));
  // Simulate successful image fetching (for image with URL) -> expect the
  // callback to be called.
  autofill_image_fetcher()->SimulateOnCardArtImageFetched(fake_url1, now,
                                                          fake_image1);
  autofill_image_fetcher()->SimulateOnCardArtImageFetched(fake_url2, now,
                                                          fake_image2);

  EXPECT_TRUE(gfx::test::AreImagesEqual(
      fake_image1, *autofill_image_fetcher()->GetCachedImageForUrl(fake_url1)));
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      fake_image2, *autofill_image_fetcher()->GetCachedImageForUrl(fake_url2)));
  EXPECT_THAT(histogram_tester.GetAllSamples("Autofill.ImageFetcher.Result"),
              BucketsAre(Bucket(false, 0), Bucket(true, 2)));
  histogram_tester.ExpectTotalCount("Autofill.ImageFetcher.RequestLatency", 2);
  histogram_tester.ExpectUniqueSample("Autofill.ImageFetcher.RequestLatency",
                                      200, 2);
}

TEST_F(AutofillImageFetcherTest, FetchImage_ResolveCardArtURL) {
  // Set the AutofillImageFetcher to replace the input `fake_url1` in
  // ResolveCardArtURL.
  GURL override_url = GURL("https://www.other.com/fake_image2");
  autofill_image_fetcher()->set_card_art_url_override(override_url);

  GURL fake_url1 = GURL("https://www.example.com/fake_image1");

  // The underlying image fetcher should only get called for the modified URL.
  EXPECT_CALL(*mock_image_fetcher(), FetchImageAndData_(fake_url1, _, _, _))
      .Times(0);
  EXPECT_CALL(*mock_image_fetcher(), FetchImageAndData_(override_url, _, _, _));
  std::vector<GURL> urls = {fake_url1};
  autofill_image_fetcher()->FetchCreditCardArtImagesForURLs(
      urls, base::span_from_ref(AutofillImageFetcherBase::ImageSize::kSmall));
}

TEST_F(AutofillImageFetcherTest, FetchImage_ResolveCardArtImage) {
  // Set the AutofillImageFetcher to replace the input `fake_image1` in
  // ResolveCardArtImage.
  gfx::Image override_image = gfx::test::CreateImage(5, 5);
  autofill_image_fetcher()->set_card_art_image_override(override_image);

  GURL fake_url1 = GURL("https://www.example.com/fake_image1");
  gfx::Image fake_image1 = gfx::test::CreateImage(1, 2);

  autofill_image_fetcher()->SimulateOnCardArtImageFetched(
      fake_url1, base::TimeTicks::Now(), fake_image1);

  // The received image should be `override_image`, because ResolveCardArtImage
  // should have changed it.
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      override_image,
      *autofill_image_fetcher()->GetCachedImageForUrl(fake_url1)));
}

TEST_F(AutofillImageFetcherTest, FetchImage_ServerFailure) {
  base::TimeTicks now = base::TimeTicks::Now();

  GURL fake_url1 = GURL("https://www.example.com/fake_image1");

  base::HistogramTester histogram_tester;
  // Expect to be called once.
  EXPECT_CALL(*mock_image_fetcher(), FetchImageAndData_(_, _, _, _));
  std::vector<GURL> urls = {fake_url1};
  autofill_image_fetcher()->FetchCreditCardArtImagesForURLs(
      urls, base::span_from_ref(AutofillImageFetcherBase::ImageSize::kSmall));

  task_environment().FastForwardBy(base::Milliseconds(200));
  // Simulate successful image fetching (for image with URL).
  autofill_image_fetcher()->SimulateOnCardArtImageFetched(fake_url1, now,
                                                          gfx::Image());

  // Empty images are not cached, so the result should be a `nullptr`.
  EXPECT_FALSE(autofill_image_fetcher()->GetCachedImageForUrl(fake_url1));
  EXPECT_THAT(histogram_tester.GetAllSamples("Autofill.ImageFetcher.Result"),
              BucketsAre(Bucket(false, 1), Bucket(true, 0)));
  histogram_tester.ExpectTotalCount("Autofill.ImageFetcher.RequestLatency", 1);
  histogram_tester.ExpectUniqueSample("Autofill.ImageFetcher.RequestLatency",
                                      200, 1);
}

TEST_F(AutofillImageFetcherTest,
       FetchImage_ServerFailure_FailureOnRepeatAttempt) {
  base::TimeTicks now = base::TimeTicks::Now();

  GURL fake_url1 = GURL("https://www.example.com/fake_image1");

  base::HistogramTester histogram_tester;
  // Expect to be called twice.
  EXPECT_CALL(*mock_image_fetcher(), FetchImageAndData_(_, _, _, _)).Times(2);
  std::vector<GURL> urls = {fake_url1};

  // Attempt 1 - Failure.
  autofill_image_fetcher()->FetchCreditCardArtImagesForURLs(
      urls, base::span_from_ref(AutofillImageFetcherBase::ImageSize::kSmall));
  task_environment().FastForwardBy(base::Milliseconds(200));
  // Simulate successful image fetching (for image with URL).
  autofill_image_fetcher()->SimulateOnCardArtImageFetched(fake_url1, now,
                                                          gfx::Image());
  // Empty images are not cached, so the result should be a `nullptr`.
  EXPECT_FALSE(autofill_image_fetcher()->GetCachedImageForUrl(fake_url1));

  // Attempt 2 - Failure.
  autofill_image_fetcher()->FetchCreditCardArtImagesForURLs(
      urls, base::span_from_ref(AutofillImageFetcherBase::ImageSize::kSmall));
  task_environment().FastForwardBy(base::Milliseconds(100));
  // Simulate successful image fetching (for image with URL).
  autofill_image_fetcher()->SimulateOnCardArtImageFetched(fake_url1, now,
                                                          gfx::Image());

  // Empty images are not cached, so the result should be a `nullptr`.
  EXPECT_FALSE(autofill_image_fetcher()->GetCachedImageForUrl(fake_url1));
  // Verify that for a given card art URL, failure is logged only once.
  EXPECT_THAT(histogram_tester.GetAllSamples("Autofill.ImageFetcher.Result"),
              BucketsAre(Bucket(false, 1), Bucket(true, 0)));
  histogram_tester.ExpectTotalCount("Autofill.ImageFetcher.RequestLatency", 1);
}

TEST_F(AutofillImageFetcherTest,
       FetchImage_ServerFailure_SuccessOnRepeatAttempt) {
  base::TimeTicks now = base::TimeTicks::Now();

  GURL fake_url1 = GURL("https://www.example.com/fake_image1");
  gfx::Image fake_image1 = GetTestImage(IDR_DEFAULT_FAVICON);
  std::map<GURL, gfx::Image> expected_images_for_failure = {
      {fake_url1, gfx::Image()}};
  std::map<GURL, gfx::Image> expected_images_for_success = {
      {fake_url1, fake_image1}};

  base::HistogramTester histogram_tester;
  // Expect to be called twice.
  EXPECT_CALL(*mock_image_fetcher(), FetchImageAndData_(_, _, _, _)).Times(2);
  std::vector<GURL> urls = {fake_url1};

  // Attempt 1 - Failure.
  autofill_image_fetcher()->FetchCreditCardArtImagesForURLs(
      urls, base::span_from_ref(AutofillImageFetcherBase::ImageSize::kSmall));
  task_environment().FastForwardBy(base::Milliseconds(200));
  // Simulate failed image fetching (for image with URL) -> expect the
  // callback to be called.
  autofill_image_fetcher()->SimulateOnCardArtImageFetched(fake_url1, now,
                                                          gfx::Image());

  // Empty images are not cached, so the result should be a `nullptr`.
  EXPECT_FALSE(autofill_image_fetcher()->GetCachedImageForUrl(fake_url1));

  // Attempt 2 - Success.
  autofill_image_fetcher()->FetchCreditCardArtImagesForURLs(
      urls, base::span_from_ref(AutofillImageFetcherBase::ImageSize::kSmall));
  task_environment().FastForwardBy(base::Milliseconds(100));
  // Simulate successful image fetching (for image with URL) -> expect the
  // callback to be called.
  autofill_image_fetcher()->SimulateOnCardArtImageFetched(fake_url1, now,
                                                          fake_image1);

  EXPECT_TRUE(gfx::test::AreImagesEqual(
      fake_image1, *autofill_image_fetcher()->GetCachedImageForUrl(fake_url1)));
  // Verify that if fetching asset for a card art URL succeeds after failing,
  // both histograms are logged.
  EXPECT_THAT(histogram_tester.GetAllSamples("Autofill.ImageFetcher.Result"),
              BucketsAre(Bucket(false, 1), Bucket(true, 1)));
  histogram_tester.ExpectTotalCount("Autofill.ImageFetcher.RequestLatency", 2);
}

TEST_F(AutofillImageFetcherTest, FetchImage_Success_SuccessOnRepeatAttempt) {
  base::TimeTicks now = base::TimeTicks::Now();

  GURL fake_url1 = GURL("https://www.example.com/fake_image1");
  gfx::Image fake_image1 = GetTestImage(IDR_DEFAULT_FAVICON);

  base::HistogramTester histogram_tester;
  // Expect to be called once because already cached images are not fetched
  // again.
  EXPECT_CALL(*mock_image_fetcher(), FetchImageAndData_(_, _, _, _));
  std::vector<GURL> urls = {fake_url1};

  // Attempt 1 - Success.
  autofill_image_fetcher()->FetchCreditCardArtImagesForURLs(
      urls, base::span_from_ref(AutofillImageFetcherBase::ImageSize::kSmall));
  task_environment().FastForwardBy(base::Milliseconds(200));
  // Simulate successful image fetching (for image with URL) -> expect the
  // callback to be called.
  autofill_image_fetcher()->SimulateOnCardArtImageFetched(fake_url1, now,
                                                          fake_image1);

  EXPECT_TRUE(gfx::test::AreImagesEqual(
      fake_image1, *autofill_image_fetcher()->GetCachedImageForUrl(fake_url1)));

  // Attempt 2 - The image has been cached already, it shouldn't be fetched
  // again.
  autofill_image_fetcher()->FetchCreditCardArtImagesForURLs(
      urls, base::span_from_ref(AutofillImageFetcherBase::ImageSize::kSmall));
  task_environment().FastForwardBy(base::Milliseconds(100));
  // Simulate successful image fetching (for image with URL) -> expect the
  // callback to be called.
  autofill_image_fetcher()->SimulateOnCardArtImageFetched(fake_url1, now,
                                                          fake_image1);

  EXPECT_TRUE(gfx::test::AreImagesEqual(
      fake_image1, *autofill_image_fetcher()->GetCachedImageForUrl(fake_url1)));
  // Verify that if multiple card art fetch attempts are made, and all of them
  // are successful, the success histogram is logged only once.
  EXPECT_THAT(histogram_tester.GetAllSamples("Autofill.ImageFetcher.Result"),
              BucketsAre(Bucket(false, 0), Bucket(true, 1)));
  histogram_tester.ExpectTotalCount("Autofill.ImageFetcher.RequestLatency", 1);
}

}  // namespace autofill
