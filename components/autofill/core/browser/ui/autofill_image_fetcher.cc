// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/autofill_image_fetcher.h"

#include "components/autofill/core/browser/data_model/credit_card_art_image.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

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
        user_data {
          type: NONE
        }
        data: "URL of the image to be fetched."
        destination: GOOGLE_OWNED_SERVICE
        internal {
          contacts {
            email: "chrome-payments-team@google.com"
          }
        }
        last_reviewed: "2023-05-12"
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

}  // namespace

void AutofillImageFetcher::FetchImagesForURLs(
    base::span<const GURL> image_urls,
    base::span<const AutofillImageFetcherBase::ImageSize> image_sizes_unused,
    base::OnceCallback<void(
        const std::vector<std::unique_ptr<CreditCardArtImage>>&)> callback) {
  if (!GetImageFetcher()) {
    std::move(callback).Run({});
    return;
  }

  // Construct a BarrierCallback and so that the inner `callback` is invoked
  // only when all the images are fetched.
  const auto barrier_callback =
      base::BarrierCallback<std::unique_ptr<CreditCardArtImage>>(
          image_urls.size(), std::move(callback));

  for (const auto& image_url : image_urls) {
    FetchImageForURL(barrier_callback, image_url);
  }
}

GURL AutofillImageFetcher::ResolveCardArtURL(const GURL& card_art_url) {
  // TODO(crbug.com/40221039): There is only one gstatic card art image we are
  // using currently, that returns as metadata when it isn't. Remove this logic
  // and append FIFE URL suffix by default when the static image is deprecated,
  // and we send rich card art instead.
  if (card_art_url.spec() == kCapitalOneCardArtUrl) {
    return card_art_url;
  }

  // A FIFE image fetching param suffix is appended to the URL. The image
  // should be center cropped and of Size(32, 20).
  return GURL(card_art_url.spec() + "=w32-h20-n");
}

gfx::Image AutofillImageFetcher::ResolveCardArtImage(
    const GURL& card_art_url,
    const gfx::Image& card_art_image) {
  return card_art_image;
}

void AutofillImageFetcher::OnCardArtImageFetched(
    base::OnceCallback<void(std::unique_ptr<CreditCardArtImage>)>
        barrier_callback,
    const GURL& card_art_url,
    const std::optional<base::TimeTicks>& fetch_image_request_timestamp,
    const gfx::Image& card_art_image,
    const image_fetcher::RequestMetadata& metadata) {
  CHECK(fetch_image_request_timestamp.has_value());

  AutofillMetrics::LogImageFetcherRequestLatency(
      base::TimeTicks::Now() - *fetch_image_request_timestamp);

  AutofillMetrics::LogImageFetchResult(/*succeeded=*/!card_art_image.IsEmpty());

  // Allow subclasses to specialize the card art image if desired.
  gfx::Image resolved_image = ResolveCardArtImage(card_art_url, card_art_image);

  std::move(barrier_callback)
      .Run(std::make_unique<CreditCardArtImage>(card_art_url, resolved_image));
}

void AutofillImageFetcher::FetchImageForURL(
    base::OnceCallback<void(std::unique_ptr<CreditCardArtImage>)>
        barrier_callback,
    const GURL& card_art_url) {
  CHECK(card_art_url.is_valid());

  // Allow subclasses to specialize the URL if desired.
  GURL resolved_url = ResolveCardArtURL(card_art_url);

  image_fetcher::ImageFetcherParams params(kCardArtImageTrafficAnnotation,
                                           kUmaClientName);
  GetImageFetcher()->FetchImage(
      resolved_url,
      base::BindOnce(&AutofillImageFetcher::OnCardArtImageFetched, GetWeakPtr(),
                     std::move(barrier_callback), card_art_url,
                     base::TimeTicks::Now()),
      std::move(params));
}

}  // namespace autofill
