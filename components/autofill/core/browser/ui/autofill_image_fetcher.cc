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
    FetchImageForURL(image_url, ImageType::kCreditCardArtImage);
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
    FetchImageForURL(image_url, ImageType::kValuableImage);
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

gfx::Image AutofillImageFetcher::ResolveImage(const GURL& image_url,
                                              const gfx::Image& image,
                                              ImageType image_type) {
  if (image.IsEmpty()) {
    return image;
  }
  switch (image_type) {
    case ImageType::kCreditCardArtImage:
      return ResolveCardArtImage(image_url, image);
    case ImageType::kPixAccountImage:
      NOTREACHED() << "Pix account images are available only on Android";
    case ImageType::kValuableImage:
      return ResolveValuableImage(image);
  }
}

AutofillImageFetcher::AutofillImageFetcher() = default;

void AutofillImageFetcher::OnImageFetched(
    const GURL& image_url,
    ImageType image_type,
    const gfx::Image& image,
    const image_fetcher::RequestMetadata& metadata) {
  AutofillMetrics::LogImageFetchResult(image_type,
                                       /* succeeded= */ !image.IsEmpty());

  // Images are cached by the resolved URLs. This allows caching the same image
  // at different scales which is useful e.g. to show images of different scales
  // on different surfaces.
  GURL resolved_url = ResolveImageURL(image_url, image_type);
  // Allow subclasses to specialize the card art image if desired.
  gfx::Image resolved_image = ResolveImage(image_url, image, image_type);

  // Unlike the `CachedImageFetcher`, the `AutofillImageFetcher` stores the
  // post-processed image in the in-memory cache.
  if (!resolved_image.IsEmpty()) {
    AutofillMetrics::LogImageFetchOverallResult(image_type,
                                                /* succeeded= */ true);
    cached_images_[resolved_url] = std::make_unique<gfx::Image>(resolved_image);
    return;
  }

  // Image fetching failed, and max retry attempts reached.
  if (fetch_attempt_counter_[resolved_url] >= kMaxFetchAttempts) {
    AutofillMetrics::LogImageFetchOverallResult(image_type,
                                                /* succeeded= */ false);
    return;
  }

  if (base::FeatureList::IsEnabled(
          features::kAutofillRetryImageFetchOnFailure)) {
    // Post a delayed task to retry the fetch.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AutofillImageFetcher::FetchImageForURL, GetWeakPtr(),
                       image_url, image_type),
        kRefetchDelay);
  }
}

void AutofillImageFetcher::FetchImageForURL(const GURL& image_url,
                                            ImageType image_type) {
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
  GetImageFetcher()->FetchImage(
      resolved_url,
      base::BindOnce(&AutofillImageFetcher::OnImageFetched, GetWeakPtr(),
                     image_url, image_type),
      std::move(params));
}

}  // namespace autofill
