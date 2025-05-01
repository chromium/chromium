// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/autofill_image_fetcher.h"

#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/gfx/image/image.h"

namespace autofill {

namespace {

constexpr char kUmaClientName[] = "AutofillImageFetcher";

constexpr net::NetworkTrafficAnnotationTag kCardArtImageTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("autofill_image_fetcher",
                                        R"(
      semantics {
        sender: "Autofill Image Fetcher"
        description:
          "Fetches images for Chrome Autofill data types like credit cards or "
          "loyalty cards stored in Chrome. Images are hosted on Google static "
          "content server, the data source may come from third parties (credit "
          "card or loyalty card issuers)."
        trigger: "When a new Autofill entry bundled with a custom image URL is "
          "sent to Chrome or when an Autofill entry in the web database is "
          "refreshed and any image is missing. Example: credit cards or "
          "loyalty cards."
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
        last_reviewed: "2025-04-14"
      }
      policy {
        cookies_allowed: NO
        setting:
          "Users can enable or disable this feature in Chromium settings by "
          "toggling 'Payment methods, offers, and addresses using Google Pay', "
          "under 'Advanced sync settings...'."
        chrome_policy {
          AutofillCreditCardEnabled {
            policy_options {mode: MANDATORY}
            AutofillCreditCardEnabled: false
          }
        }
      })");

// Time between fetch attempts.
static constexpr base::TimeDelta kRefetchDelay = base::Minutes(2);

// Maximum number of times to attempt fetching an image.
static constexpr int kMaxFetchAttempts = 2;

}  // namespace

AutofillImageFetcher::~AutofillImageFetcher() = default;

void AutofillImageFetcher::FetchCreditCardArtImagesForURLs(
    base::span<const GURL> image_urls,
    base::span<const AutofillImageFetcherBase::ImageSize> image_sizes_unused) {
  if (!GetImageFetcher()) {
    return;
  }

  for (const auto& image_url : image_urls) {
    FetchImageForURL(
        image_url, ImageType::kCreditCardArtImage,
        base::BindOnce(&AutofillImageFetcher::OnCardArtImageFetched,
                       GetWeakPtr(), image_url));
  }
}

// Only implemented in Android clients. Pay with Pix is only available in Chrome
// on Android.
void AutofillImageFetcher::FetchPixAccountImagesForURLs(
    base::span<const GURL> image_urls) {
  NOTREACHED();
}

void AutofillImageFetcher::FetchValuableImagesForURLs(
    base::span<const GURL> image_urls) {
  if (!GetImageFetcher()) {
    return;
  }

  for (const auto& image_url : image_urls) {
    FetchImageForURL(
        image_url, ImageType::kValuableImage,
        base::BindOnce(&AutofillImageFetcher::OnValuableImageFetched,
                       GetWeakPtr(), image_url));
  }
}

const gfx::Image* AutofillImageFetcher::GetCachedImageForUrl(
    const GURL& image_url,
    ImageType image_type) const {
  GURL resolved_url = ResolveImageURL(image_url, image_type);
  auto it = cached_images_.find(resolved_url);
  if (it == cached_images_.end() || it->second->IsEmpty()) {
    return nullptr;
  }
  return it->second.get();
}

gfx::Image AutofillImageFetcher::ResolveCardArtImage(
    const GURL& card_art_url,
    const gfx::Image& card_art_image) {
  return card_art_image;
}

AutofillImageFetcher::AutofillImageFetcher() = default;

void AutofillImageFetcher::OnCardArtImageFetched(
    const GURL& card_art_url,
    const gfx::Image& card_art_image,
    const image_fetcher::RequestMetadata& metadata) {
  AutofillMetrics::LogImageFetchResult(
      AutofillImageFetcherBase::ImageType::kCreditCardArtImage,
      /* succeeded= */ !card_art_image.IsEmpty());

  // Images are cached by the resolved URLs. This allows caching the same image
  // at different scales which is useful e.g. to show images of different scales
  // on different surfaces.
  GURL resolved_url =
      ResolveImageURL(card_art_url, ImageType::kCreditCardArtImage);
  // Allow subclasses to specialize the card art image if desired.
  gfx::Image resolved_image = ResolveCardArtImage(card_art_url, card_art_image);

  // Unlike the `CachedImageFetcher`, the `AutofillImageFetcher` stores the
  // post-processed image in the in-memory cache.
  if (!resolved_image.IsEmpty()) {
    AutofillMetrics::LogImageFetchOverallResult(
        AutofillImageFetcherBase::ImageType::kCreditCardArtImage,
        /* succeeded= */ true);

    cached_images_[resolved_url] = std::make_unique<gfx::Image>(resolved_image);
    return;
  }

  // Image fetching failed, and max retry attempts reached.
  if (fetch_attempt_counter_[resolved_url] >= kMaxFetchAttempts) {
    AutofillMetrics::LogImageFetchOverallResult(
        AutofillImageFetcherBase::ImageType::kCreditCardArtImage,
        /* succeeded= */ false);
    return;
  }

  if (base::FeatureList::IsEnabled(
          features::kAutofillRetryImageFetchOnFailure)) {
    // Post a delayed task to retry the fetch.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &AutofillImageFetcher::FetchImageForURL, GetWeakPtr(), card_art_url,
            ImageType::kCreditCardArtImage,
            base::BindOnce(&AutofillImageFetcher::OnCardArtImageFetched,
                           GetWeakPtr(), card_art_url)),
        kRefetchDelay);
  }
}

void AutofillImageFetcher::OnValuableImageFetched(
    const GURL& image_url,
    const gfx::Image& valuable_image,
    const image_fetcher::RequestMetadata& metadata) {
  if (!valuable_image.IsEmpty()) {
    // Images are cached by the resolved URLs.
    GURL resolved_url = ResolveImageURL(image_url, ImageType::kValuableImage);
    cached_images_[resolved_url] = std::make_unique<gfx::Image>(valuable_image);
  }
}

void AutofillImageFetcher::FetchImageForURL(
    const GURL& image_url,
    ImageType image_type,
    image_fetcher::ImageFetcherCallback callback) {
  CHECK(image_url.is_valid());

  // Don't fetch the image if the in-memory cache already contains it.
  if (GetCachedImageForUrl(image_url, image_type)) {
    return;
  }

  // Allow subclasses to specialize the URL if desired.
  GURL resolved_url = ResolveImageURL(image_url, image_type);

  // Update attempt counter for the URL.
  // Note: std::map::operator[] default-constructs the value (to 0) if
  // `resolved_url` is not present in the map yet.
  fetch_attempt_counter_[resolved_url]++;

  image_fetcher::ImageFetcherParams params(kCardArtImageTrafficAnnotation,
                                           kUmaClientName);
  GetImageFetcher()->FetchImage(resolved_url, std::move(callback),
                                std::move(params));
}

}  // namespace autofill
