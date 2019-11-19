// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/cached_image_fetcher.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/ntp_snippets/remote/remote_suggestions_database.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"

namespace ntp_snippets {

namespace {

constexpr char kImageFetcherUmaClientName[] = "NtpSnippets";

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("remote_suggestions_provider", R"(
        semantics {
          sender: "Content Suggestion Thumbnail Fetch"
          description:
            "Retrieves thumbnails for content suggestions, for display on the "
            "New Tab page or Chrome Home."
          trigger:
            "Triggered when the user looks at a content suggestion (and its "
            "thumbnail isn't cached yet)."
          data: "None."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "Currently not available, but in progress: crbug.com/703684"
        chrome_policy {
          NTPContentSuggestionsEnabled {
            policy_options {mode: MANDATORY}
            NTPContentSuggestionsEnabled: false
          }
        }
      })");
}  // namespace

CachedImageFetcher::CachedImageFetcher(
    std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher,
    PrefService* pref_service,
    RemoteSuggestionsDatabase* database)
    : image_fetcher_(std::move(image_fetcher)),
      database_(database),
      thumbnail_requests_throttler_(
          pref_service,
          RequestThrottler::RequestType::CONTENT_SUGGESTION_THUMBNAIL) {}

CachedImageFetcher::~CachedImageFetcher() {}

void CachedImageFetcher::FetchSuggestionImage(
    const ContentSuggestion::ID& suggestion_id,
    const GURL& url,
    ImageDataFetchedCallback image_data_callback,
    ImageFetchedCallback image_callback) {
  database_->LoadImage(
      suggestion_id.id_within_category(),
      base::BindOnce(&CachedImageFetcher::OnImageFetchedFromDatabase,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(image_data_callback), std::move(image_callback),
                     suggestion_id, url));
}

void CachedImageFetcher::OnImageDecodingDone(
    ImageFetchedCallback callback,
    const std::string& id_within_category,
    const gfx::Image& image,
    const image_fetcher::RequestMetadata& metadata) {
  std::move(callback).Run(image);
}

void CachedImageFetcher::OnImageFetchingDone(
    ImageFetchedCallback callback,
    const gfx::Image& image,
    const image_fetcher::RequestMetadata& metadata) {
  std::move(callback).Run(image);
}

void CachedImageFetcher::OnImageFetchedFromDatabase(
    ImageDataFetchedCallback image_data_callback,
    ImageFetchedCallback image_callback,
    const ContentSuggestion::ID& suggestion_id,
    const GURL& url,
    std::string data) {  // SnippetImageCallback requires by-value.
  if (data.empty()) {
    // Fetching from the DB failed; start a network fetch.
    FetchImageFromNetwork(suggestion_id, url, std::move(image_data_callback),
                          std::move(image_callback));
    return;
  }

  if (image_data_callback) {
    std::move(image_data_callback).Run(data);
  }

  if (image_callback) {
    image_fetcher_->GetImageDecoder()->DecodeImage(
        data,
        // We're not dealing with multi-frame images.
        /*desired_image_frame_size=*/gfx::Size(),
        base::BindOnce(&CachedImageFetcher::OnImageDecodedFromDatabase,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(image_callback), suggestion_id, url));
  }
}

void CachedImageFetcher::OnImageDecodedFromDatabase(
    ImageFetchedCallback callback,
    const ContentSuggestion::ID& suggestion_id,
    const GURL& url,
    const gfx::Image& image) {
  if (!image.IsEmpty()) {
    std::move(callback).Run(image);
    return;
  }
  // If decoding the image failed, delete the DB entry.
  database_->DeleteImage(suggestion_id.id_within_category());
  FetchImageFromNetwork(suggestion_id, url, ImageDataFetchedCallback(),
                        std::move(callback));
}

void CachedImageFetcher::FetchImageFromNetwork(
    const ContentSuggestion::ID& suggestion_id,
    const GURL& url,
    ImageDataFetchedCallback image_data_callback,
    ImageFetchedCallback image_callback) {
  if (url.is_empty() || !thumbnail_requests_throttler_.DemandQuotaForRequest(
                            /*interactive_request=*/true)) {
    // Return an empty image. Directly, this is never synchronous with the
    // original FetchSuggestionImage() call - an asynchronous database query has
    // happened in the meantime.
    if (image_data_callback) {
      std::move(image_data_callback).Run(std::string());
    }
    if (image_callback) {
      std::move(image_callback).Run(gfx::Image());
    }
    return;
  }
  // Image decoding callback only set when requested.
  image_fetcher::ImageFetcherCallback decode_callback;
  if (image_callback) {
    decode_callback = base::BindOnce(&CachedImageFetcher::OnImageFetchingDone,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     std::move(image_callback));
  }

  image_fetcher::ImageFetcherParams params(kTrafficAnnotation,
                                           kImageFetcherUmaClientName);
  image_fetcher_->FetchImageAndData(
      url,
      base::BindOnce(&CachedImageFetcher::SaveImageAndInvokeDataCallback,
                     weak_ptr_factory_.GetWeakPtr(),
                     suggestion_id.id_within_category(),
                     std::move(image_data_callback)),
      std::move(decode_callback), std::move(params));
}

void CachedImageFetcher::SaveImageAndInvokeDataCallback(
    const std::string& id_within_category,
    ImageDataFetchedCallback callback,
    const std::string& image_data,
    const image_fetcher::RequestMetadata& request_metadata) {
  if (!image_data.empty()) {
    database_->SaveImage(id_within_category, image_data);
  }
  if (callback) {
    std::move(callback).Run(image_data);
  }
}

}  // namespace ntp_snippets
