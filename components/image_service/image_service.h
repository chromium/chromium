// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_SERVICE_IMAGE_SERVICE_H_
#define COMPONENTS_IMAGE_SERVICE_IMAGE_SERVICE_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/image_service/mojom/image_service.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/sync/driver/sync_service.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"

namespace image_service {

// Used to get the image URL associated with a cluster. It doesn't actually
// fetch the image, that's up to the UI to do.
class ImageService : public KeyedService {
 public:
  using ResultCallback = base::OnceCallback<void(const GURL& image_url)>;

  ImageService(
      std::unique_ptr<AutocompleteProviderClient> autocomplete_provider_client,
      syncer::SyncService* sync_service);
  ImageService(const ImageService&) = delete;
  ImageService& operator=(const ImageService&) = delete;

  ~ImageService() override;

  // Gets a weak pointer to this service. Used when UIs want to create an
  // object whose lifetime might exceed the service.
  base::WeakPtr<ImageService> GetWeakPtr();

  // Fetches an image appropriate for `page_url`, returning the result
  // asynchronously to `callback`. The callback is always invoked. If there are
  // no images available, it is invoked with an empty GURL result.
  void FetchImageFor(mojom::ClientId client_id,
                     const GURL& page_url,
                     const mojom::Options& options,
                     ResultCallback callback);

 private:
  class SuggestEntityImageURLFetcher;

  // Fetches an image appropriate for `search_query` and `entity_id`, returning
  // the result asynchronously to `callback`.
  void FetchImageFor(const std::u16string& search_query,
                     const std::string& entity_id,
                     ResultCallback callback);

  // Callback for `FetchImageFor`.
  void OnImageFetched(std::unique_ptr<SuggestEntityImageURLFetcher> fetcher,
                      ResultCallback callback,
                      const GURL& image_url);

  std::unique_ptr<AutocompleteProviderClient> autocomplete_provider_client_;
  std::unique_ptr<unified_consent::UrlKeyedDataCollectionConsentHelper>
      url_consent_helper_;

  base::WeakPtrFactory<ImageService> weak_factory_{this};
};

}  // namespace image_service

#endif  // COMPONENTS_IMAGE_SERVICE_IMAGE_SERVICE_H_
