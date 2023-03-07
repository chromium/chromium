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
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/sync/driver/sync_service.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"

namespace optimization_guide {
class NewOptimizationGuideDecider;
}  // namespace optimization_guide

namespace image_service {

// Used to get the image URL associated with a cluster. It doesn't actually
// fetch the image, that's up to the UI to do.
class ImageService : public KeyedService {
 public:
  using ResultCallback = base::OnceCallback<void(const GURL& image_url)>;

  ImageService(
      std::unique_ptr<AutocompleteProviderClient> autocomplete_provider_client,
      optimization_guide::NewOptimizationGuideDecider* opt_guide,
      syncer::SyncService* sync_service);
  ImageService(const ImageService&) = delete;
  ImageService& operator=(const ImageService&) = delete;

  ~ImageService() override;

  // Gets a weak pointer to this service. Used when UIs want to create an
  // object whose lifetime might exceed the service.
  base::WeakPtr<ImageService> GetWeakPtr();

  // Returns true if `client_id` has permission to fetch images.
  bool HasPermissionToFetchImage(mojom::ClientId client_id) const;

  // Fetches an image appropriate for `page_url`, returning the result
  // asynchronously to `callback`. The callback is always invoked. If there are
  // no images available, it is invoked with an empty GURL result.
  void FetchImageFor(mojom::ClientId client_id,
                     const GURL& page_url,
                     const mojom::Options& options,
                     ResultCallback callback);

 private:
  class SuggestEntityImageURLFetcher;

  // Fetches an image from Suggest appropriate for `search_query` and
  // `entity_id`, returning the result asynchronously to `callback`.
  void FetchSuggestImage(const std::u16string& search_query,
                         const std::string& entity_id,
                         ResultCallback callback);

  // Callback for `FetchSuggestImage`.
  void OnSuggestImageFetched(
      std::unique_ptr<SuggestEntityImageURLFetcher> fetcher,
      ResultCallback callback,
      const GURL& image_url);

  // Fetches an image from Optimization Guide appropriate for the parameters.
  virtual void FetchOptimizationGuideImage(mojom::ClientId client_id,
                                           const GURL& page_url,
                                           ResultCallback callback);

  // Callback for `FetchOptimizationGuideImage`.
  void OnOptimizationGuideImageFetched(
      ResultCallback callback,
      const GURL& url,
      const base::flat_map<
          optimization_guide::proto::OptimizationType,
          optimization_guide::OptimizationGuideDecisionWithMetadata>&
          decisions);

  // Autocomplete provider client used to make Suggest image requests.
  std::unique_ptr<AutocompleteProviderClient> autocomplete_provider_client_;

  // Non-owning pointer to the Optimization Guide source of images.
  // Will be left as nullptr if the OptimizationGuide feature is disabled.
  raw_ptr<optimization_guide::NewOptimizationGuideDecider> opt_guide_ = nullptr;

  // The History consent filter, used for most clients.
  std::unique_ptr<unified_consent::UrlKeyedDataCollectionConsentHelper>
      personalized_data_collection_consent_helper_;

  base::WeakPtrFactory<ImageService> weak_factory_{this};
};

}  // namespace image_service

#endif  // COMPONENTS_IMAGE_SERVICE_IMAGE_SERVICE_H_
