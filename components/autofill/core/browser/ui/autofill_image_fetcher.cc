// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/autofill_image_fetcher.h"

#include "components/autofill/core/browser/data_model/credit_card_art_image.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_tick_clock.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/gfx/image/image.h"

namespace autofill {

namespace {

constexpr char kUmaClientName[] = "AutofillImageFetcher";

constexpr net::NetworkTrafficAnnotationTag kCardArtImageTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("autofill_image_fetcher_card_art_image",
                                        R"(
      semantics {
        sender: "Autofill Image Fetcher"
        description:
          "Fetches customized card art images for credit cards stored in "
          "Chrome. Images are hosted on Google static content server, "
          "the data source may come from third parties (credit card issuers)."
        trigger: "When new credit card data is sent to Chrome if the card "
          "has a related card art image, and when the credit card data in "
          "the web database is refreshed and any card art image is missing."
        data: "URL of the image to be fetched."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting:
          "Users can enable or disable this feature in Chromium settings by "
          "toggling 'Credit cards and addresses using Google Payments', "
          "under 'Advanced sync settings...'."
        chrome_policy {
          AutoFillEnabled {
            policy_options {mode: MANDATORY}
            AutoFillEnabled: false
          }
        }
      })");

// Defines the expiration of the fetched image in the disk cache of the image
// fetcher.
constexpr base::TimeDelta kDiskCacheExpiry = base::Minutes(10);

}  // namespace

ImageFetchOperation::ImageFetchOperation(size_t image_count,
                                         CardArtImagesFetchedCallback callback)
    : pending_request_count_(image_count),
      all_fetches_complete_callback_(std::move(callback)) {}

void ImageFetchOperation::ImageFetched(
    const GURL& card_art_url,
    const gfx::Image& card_art_image,
    const absl::optional<base::TimeTicks>& fetch_image_request_timestamp) {
  // In case of invalid url, fetch_image_request_timestamp is nullopt, and hence
  // we don't report any UMA metrics.
  if (fetch_image_request_timestamp.has_value()) {
    AutofillMetrics::LogImageFetcherRequestLatency(
        AutofillTickClock::NowTicks() - *fetch_image_request_timestamp);
  }
  AutofillMetrics::LogImageFetchResult(/*succeeded=*/!card_art_image.IsEmpty());
  pending_request_count_--;

  if (!card_art_image.IsEmpty()) {
    auto credit_card_art_image = std::make_unique<CreditCardArtImage>();
    credit_card_art_image->card_art_url = card_art_url;
    credit_card_art_image->card_art_image = card_art_image;

    fetched_card_art_images_.emplace_back(std::move(credit_card_art_image));
  }

  if (pending_request_count_ == 0U) {
    std::move(all_fetches_complete_callback_)
        .Run(std::move(fetched_card_art_images_));
  }
}

ImageFetchOperation::~ImageFetchOperation() = default;

AutofillImageFetcher::AutofillImageFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<image_fetcher::ImageDecoder> image_decoder) {
  if (url_loader_factory && image_decoder) {
    image_fetcher_ = std::make_unique<image_fetcher::ImageFetcherImpl>(
        std::move(image_decoder), std::move(url_loader_factory));
  }
}

AutofillImageFetcher::~AutofillImageFetcher() = default;

void AutofillImageFetcher::FetchImagesForUrls(
    const std::vector<GURL>& card_art_urls,
    CardArtImagesFetchedCallback callback) {
  if (!image_fetcher_) {
    std::move(callback).Run({});
    return;
  }

  auto image_fetcher_operation = base::MakeRefCounted<ImageFetchOperation>(
      card_art_urls.size(), std::move(callback));

  for (const auto& card_art_url : card_art_urls)
    FetchImageForUrl(image_fetcher_operation, card_art_url);
}

void AutofillImageFetcher::FetchImageForUrl(
    const scoped_refptr<ImageFetchOperation>& operation,
    const GURL& card_art_url) {
  if (!card_art_url.is_valid()) {
    OnCardArtImageFetched(operation, card_art_url, absl::nullopt, gfx::Image(),
                          image_fetcher::RequestMetadata());
    return;
  }

  image_fetcher::ImageFetcherParams params(kCardArtImageTrafficAnnotation,
                                           kUmaClientName);
  params.set_hold_for_expiration_interval(kDiskCacheExpiry);
  image_fetcher_->FetchImage(
      card_art_url,
      base::BindOnce(&AutofillImageFetcher::OnCardArtImageFetched, operation,
                     card_art_url, AutofillTickClock::NowTicks()),
      std::move(params));
}

// static
void AutofillImageFetcher::OnCardArtImageFetched(
    const scoped_refptr<ImageFetchOperation>& operation,
    const GURL& card_art_url,
    const absl::optional<base::TimeTicks>& fetch_image_request_timestamp,
    const gfx::Image& card_art_image,
    const image_fetcher::RequestMetadata& metadata) {
  operation->ImageFetched(card_art_url, card_art_image,
                          fetch_image_request_timestamp);
}

}  // namespace autofill
