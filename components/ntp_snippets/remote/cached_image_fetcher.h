// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_REMOTE_CACHED_IMAGE_FETCHER_H_
#define COMPONENTS_NTP_SNIPPETS_REMOTE_CACHED_IMAGE_FETCHER_H_

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/ntp_snippets/callbacks.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/remote/request_throttler.h"

class PrefService;

namespace gfx {
class Image;
}  // namespace gfx

namespace image_fetcher {
class ImageFetcher;
struct RequestMetadata;
}  // namespace image_fetcher

namespace ntp_snippets {

class RemoteSuggestionsDatabase;

// CachedImageFetcher takes care of fetching images from the network and caching
// them in the database.
class CachedImageFetcher {
 public:
  // |pref_service| and |database| need to outlive the created image fetcher
  // instance.
  CachedImageFetcher(std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher,
                     PrefService* pref_service,
                     RemoteSuggestionsDatabase* database);
  virtual ~CachedImageFetcher();

  // Fetches the image for a suggestion. The fetcher will first issue a lookup
  // to the underlying cache with a fallback to the network.
  virtual void FetchSuggestionImage(
      const ContentSuggestion::ID& suggestion_id,
      const GURL& image_url,
      ImageDataFetchedCallback image_data_callback,
      ImageFetchedCallback image_callback);

 private:
  void OnImageDataFetched(const std::string& id_within_category,
                          const std::string& image_data,
                          const image_fetcher::RequestMetadata& metadata);

  void OnImageDecodingDone(ImageFetchedCallback callback,
                           const std::string& id_within_category,
                           const gfx::Image& image,
                           const image_fetcher::RequestMetadata& metadata);

  void OnImageFetchingDone(ImageFetchedCallback callback,
                           const gfx::Image& image,
                           const image_fetcher::RequestMetadata& metadata);

  void OnImageFetchedFromDatabase(
      ImageDataFetchedCallback image_data_callback,
      ImageFetchedCallback image_callback,
      const ContentSuggestion::ID& suggestion_id,
      const GURL& image_url,
      // SnippetImageCallback requires by-value (not const ref).
      std::string data);

  void OnImageDecodedFromDatabase(ImageFetchedCallback callback,
                                  const ContentSuggestion::ID& suggestion_id,
                                  const GURL& url,
                                  const gfx::Image& image);

  void FetchImageFromNetwork(const ContentSuggestion::ID& suggestion_id,
                             const GURL& url,
                             ImageDataFetchedCallback image_data_callback,
                             ImageFetchedCallback image_callback);
  void SaveImageAndInvokeDataCallback(
      const std::string& id_within_category,
      ImageDataFetchedCallback callback,
      const std::string& image_data,
      const image_fetcher::RequestMetadata& request_metadata);

  std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher_;
  RemoteSuggestionsDatabase* database_;
  // Request throttler for limiting requests to thumbnail images.
  RequestThrottler thumbnail_requests_throttler_;

  base::WeakPtrFactory<CachedImageFetcher> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CachedImageFetcher);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_REMOTE_CACHED_IMAGE_FETCHER_H_
