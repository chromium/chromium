// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/autofill_image_fetcher.h"

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_art_image.h"
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

class TestAutofillImageFetcher : public AutofillImageFetcher {
 public:
  explicit TestAutofillImageFetcher()
      : mock_image_fetcher_(
            std::make_unique<image_fetcher::MockImageFetcher>()) {}
  ~TestAutofillImageFetcher() override = default;

  image_fetcher::MockImageFetcher* mock_image_fetcher() const {
    return mock_image_fetcher_.get();
  }

  void set_card_art_url_override(const GURL& card_art_url_override) {
    card_art_url_override_ = card_art_url_override;
  }
  void set_card_art_image_override(const gfx::Image& card_art_image_override) {
    card_art_image_override_ = card_art_image_override;
  }

  void SimulateOnImageFetched(
      base::OnceCallback<void(std::unique_ptr<CreditCardArtImage>)>
          barrier_callback,
      const GURL& url,
      const std::optional<base::TimeTicks>& fetch_image_request_timestamp,
      const gfx::Image& image) {
    OnCardArtImageFetched(std::move(barrier_callback), url,
                          fetch_image_request_timestamp, image,
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
  base::WeakPtrFactory<TestAutofillImageFetcher> weak_ptr_factory_{this};
};

class AutofillImageFetcherTest : public testing::Test {
 public:
  AutofillImageFetcherTest()
      : autofill_image_fetcher_(std::make_unique<TestAutofillImageFetcher>()) {}

  void ValidateResult(const std::map<GURL, gfx::Image>& received_images,
                      const std::map<GURL, gfx::Image>& expected_images) {
    ASSERT_EQ(expected_images.size(), received_images.size());
    for (const auto& [expected_url, expected_image] : expected_images) {
      ASSERT_TRUE(base::Contains(received_images, expected_url));
      const gfx::Image& received_image = received_images.at(expected_url);
      EXPECT_TRUE(gfx::test::AreImagesEqual(expected_image, received_image));
    }
  }

  gfx::Image& GetTestImage(int resource_id) {
    return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
        resource_id);
  }

  image_fetcher::MockImageFetcher* mock_image_fetcher() {
    return autofill_image_fetcher_->mock_image_fetcher();
  }

  TestAutofillImageFetcher* autofill_image_fetcher() {
    return autofill_image_fetcher_.get();
  }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestAutofillImageFetcher> autofill_image_fetcher_;
};

TEST_F(AutofillImageFetcherTest, FetchImage_Success) {
  base::TimeTicks now = base::TimeTicks::Now();

  // The credit card network images cannot be found in the tests, but it should
  // be okay since we don't care what the images are.
  gfx::Image fake_image1 = GetTestImage(IDR_DEFAULT_FAVICON);
  gfx::Image fake_image2 = GetTestImage(IDR_DEFAULT_FAVICON);
  GURL fake_url1 = GURL("https://www.example.com/fake_image1");
  GURL fake_url2 = GURL(kCapitalOneCardArtUrl);

  std::map<GURL, gfx::Image> expected_images = {{fake_url1, fake_image1},
                                                {fake_url2, fake_image2}};

  // Expect callback to be called with some received images.
  std::map<GURL, gfx::Image> received_images;
  const auto callback = base::BindLambdaForTesting(
      [&](const std::vector<std::unique_ptr<CreditCardArtImage>>&
              card_art_images) {
        for (auto& entry : card_art_images) {
          received_images[entry->card_art_url] = entry->card_art_image;
        }
      });
  const auto barrier_callback =
      base::BarrierCallback<std::unique_ptr<CreditCardArtImage>>(
          2U, std::move(callback));

  base::HistogramTester histogram_tester;
  // Expect to be called twice. The 'normal' URL should have a size appended to
  // it, whilst the capitalone image is 'special' and does not.
  EXPECT_CALL(
      *mock_image_fetcher(),
      FetchImageAndData_(GURL(fake_url1.spec() + "=w32-h20-n"), _, _, _))
      .Times(1);
  EXPECT_CALL(*mock_image_fetcher(), FetchImageAndData_(fake_url2, _, _, _))
      .Times(1);
  std::vector<GURL> urls = {fake_url1, fake_url2};
  autofill_image_fetcher()->FetchImagesForURLs(
      urls, base::span({AutofillImageFetcherBase::ImageSize::kSmall}),
      base::DoNothing());

  // Advance the time to make the latency values more realistic.
  task_environment().FastForwardBy(base::Milliseconds(200));
  // Simulate successful image fetching (for image with URL) -> expect the
  // callback to be called.
  autofill_image_fetcher()->SimulateOnImageFetched(barrier_callback, fake_url1,
                                                   now, fake_image1);
  autofill_image_fetcher()->SimulateOnImageFetched(barrier_callback, fake_url2,
                                                   now, fake_image2);

  ValidateResult(std::move(received_images), expected_images);
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
  EXPECT_CALL(*mock_image_fetcher(), FetchImageAndData_(override_url, _, _, _))
      .Times(1);
  std::vector<GURL> urls = {fake_url1};
  autofill_image_fetcher()->FetchImagesForURLs(
      urls, base::span({AutofillImageFetcherBase::ImageSize::kSmall}),
      base::DoNothing());
}

TEST_F(AutofillImageFetcherTest, FetchImage_ResolveCardArtImage) {
  // Set the AutofillImageFetcher to replace the input `fake_image1` in
  // ResolveCardArtImage.
  gfx::Image override_image = gfx::test::CreateImage(5, 5);
  autofill_image_fetcher()->set_card_art_image_override(override_image);

  GURL fake_url1 = GURL("https://www.example.com/fake_image1");
  gfx::Image fake_image1 = gfx::test::CreateImage(1, 2);

  std::map<GURL, gfx::Image> received_images;
  const auto callback = base::BindLambdaForTesting(
      [&](const std::vector<std::unique_ptr<CreditCardArtImage>>&
              card_art_images) {
        for (auto& entry : card_art_images) {
          received_images[entry->card_art_url] = entry->card_art_image;
        }
      });
  const auto barrier_callback =
      base::BarrierCallback<std::unique_ptr<CreditCardArtImage>>(
          1U, std::move(callback));

  autofill_image_fetcher()->SimulateOnImageFetched(
      barrier_callback, fake_url1, base::TimeTicks::Now(), fake_image1);

  // The received image should be `override_image`, because ResolveCardArtImage
  // should have changed it.
  ASSERT_EQ(1U, received_images.size());
  ASSERT_TRUE(received_images.contains(fake_url1));
  EXPECT_TRUE(
      gfx::test::AreImagesEqual(override_image, received_images[fake_url1]));
}

TEST_F(AutofillImageFetcherTest, FetchImage_ServerFailure) {
  base::TimeTicks now = base::TimeTicks::Now();

  GURL fake_url1 = GURL("https://www.example.com/fake_image1");
  std::map<GURL, gfx::Image> expected_images = {{fake_url1, gfx::Image()}};

  // Expect callback to be called with some received images.
  std::map<GURL, gfx::Image> received_images;
  const auto callback = base::BindLambdaForTesting(
      [&](const std::vector<std::unique_ptr<CreditCardArtImage>>&
              card_art_images) {
        for (auto& entry : card_art_images) {
          received_images[entry->card_art_url] = entry->card_art_image;
        }
      });
  const auto barrier_callback =
      base::BarrierCallback<std::unique_ptr<CreditCardArtImage>>(
          1U, std::move(callback));

  base::HistogramTester histogram_tester;
  // Expect to be called once.
  EXPECT_CALL(*mock_image_fetcher(), FetchImageAndData_(_, _, _, _)).Times(1);
  std::vector<GURL> urls = {fake_url1};
  autofill_image_fetcher()->FetchImagesForURLs(
      urls, base::span({AutofillImageFetcherBase::ImageSize::kSmall}),
      base::DoNothing());

  task_environment().FastForwardBy(base::Milliseconds(200));
  // Simulate failed image fetching (for image with URL) -> expect the
  // callback to be called.
  autofill_image_fetcher()->SimulateOnImageFetched(barrier_callback, fake_url1,
                                                   now, gfx::Image());

  ValidateResult(std::move(received_images), expected_images);
  EXPECT_THAT(histogram_tester.GetAllSamples("Autofill.ImageFetcher.Result"),
              BucketsAre(Bucket(false, 1), Bucket(true, 0)));
  histogram_tester.ExpectTotalCount("Autofill.ImageFetcher.RequestLatency", 1);
  histogram_tester.ExpectUniqueSample("Autofill.ImageFetcher.RequestLatency",
                                      200, 1);
}

}  // namespace autofill
