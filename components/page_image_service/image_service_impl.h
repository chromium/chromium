// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_IMAGE_SERVICE_IMAGE_SERVICE_IMPL_H_
#define COMPONENTS_PAGE_IMAGE_SERVICE_IMAGE_SERVICE_IMPL_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/page_image_service/image_service.h"
#include "components/page_image_service/mojom/page_image_service.mojom.h"
#include "components/sync/service/sync_service.h"

class AutocompleteSchemeClassifier;
class RemoteSuggestionsService;
class SearchTermsData;
class TemplateURL;
class TemplateURLService;

namespace optimization_guide {
class OptimizationGuideDecider;
}  // namespace optimization_guide

namespace page_image_service {

class ImageServiceConsentHelper;
enum class PageImageServiceConsentStatus;

// Through my manual testing, 16ms (which is about a frame at 60hz) allowed
// for decent aggregation without introducing any perceptible lag.
constexpr base::TimeDelta kOptimizationGuideBatchingTimeout =
    base::Milliseconds(16);

// Used to get the image URL associated with a cluster. It doesn't actually
// fetch the image, that's up to the UI to do.
class ImageServiceImpl : public ImageService {
 public:
  ImageServiceImpl(TemplateURLService* template_url_service,
                   RemoteSuggestionsService* remote_suggestions_service,
                   optimization_guide::OptimizationGuideDecider* opt_guide,
                   syncer::SyncService* sync_service,
                   std::unique_ptr<AutocompleteSchemeClassifier>
                       autocomplete_scheme_classifier);
  ImageServiceImpl(const ImageServiceImpl&) = delete;
  ImageServiceImpl& operator=(const ImageServiceImpl&) = delete;

  ~ImageServiceImpl() override;

  // ImageService implementation:
  void FetchImageFor(mojom::ClientId client_id,
                     const GURL& page_url,
                     const mojom::Options& options,
                     ResultCallback callback) override;
  base::WeakPtr<ImageService> GetWeakPtr() override;

  // Asynchronously returns whether `client_id` has consent to fetch an image.
  // Public for testing purposes only.
  void GetConsentToFetchImage(
      mojom::ClientId client_id,
      base::OnceCallback<void(PageImageServiceConsentStatus)> callback);

 private:
  class SuggestEntityImageURLFetcher;

  struct OptGuideRequest {
    OptGuideRequest();
    ~OptGuideRequest();
    OptGuideRequest(OptGuideRequest&& other);
    GURL url;
    ResultCallback callback;
  };

  // Callback to `GetConsentToFetchImage`, proceeds to call the appropriate
  // backend.
  void OnConsentResult(mojom::ClientId client_id,
                       const GURL& page_url,
                       const mojom::Options& options,
                       ResultCallback callback,
                       PageImageServiceConsentStatus status);

  // Fetches an image from Suggest appropriate for `search_query` and
  // `entity_id`, returning the result asynchronously to `callback`.
  void FetchSuggestImage(const TemplateURL* template_url,
                         const SearchTermsData& search_terms_data,
                         mojom::ClientId client_id,
                         const std::u16string& search_query,
                         const std::string& entity_id,
                         ResultCallback callback);

  // Callback for `FetchSuggestImage`.
  void OnSuggestImageFetched(
      std::unique_ptr<SuggestEntityImageURLFetcher> fetcher,
      ResultCallback callback,
      const GURL& image_url);

  // Fetches an image from Optimization Guide appropriate for the parameters.
  // This internally puts the request into a queue for batching purposes.
  virtual void FetchOptimizationGuideImage(mojom::ClientId client_id,
                                           const GURL& page_url,
                                           ResultCallback callback);

  // Processes all the enqueued optimization guide requests for
  // `client_id` in a single batch.
  void ProcessAllBatchedOptimizationGuideRequests(mojom::ClientId client_id);

  // Callback for `ProcessAllBatchedOptimizationGuideRequests`.
  // Takes ownership of `original_requests`, which has all the original
  // `OnceCallback`s that this method fulfills.
  void OnOptimizationGuideImageFetched(
      mojom::ClientId client_id,
      const GURL& url,
      const base::flat_map<
          optimization_guide::proto::OptimizationType,
          optimization_guide::OptimizationGuideDecisionWithMetadata>&
          decisions);

  // Non-owning pointers to service dependencies. They may be nullptr.
  raw_ptr<TemplateURLService> template_url_service_ = nullptr;
  raw_ptr<RemoteSuggestionsService> remote_suggestions_service_ = nullptr;
  raw_ptr<optimization_guide::OptimizationGuideDecider> opt_guide_ = nullptr;

  // The History consent throttle, used for most clients.
  std::unique_ptr<ImageServiceConsentHelper> history_consent_helper_;

  // The Bookmarks consent throttle.
  std::unique_ptr<ImageServiceConsentHelper> bookmarks_consent_helper_;

  // Used to make proper suggest requests.
  std::unique_ptr<AutocompleteSchemeClassifier> autocomplete_scheme_classifier_;

  // Stores all the Optimization Guide requests that are still waiting to be
  // aggregated into a batch and sent. When sent in a batch, the requests are
  // moved to `sent_opt_guide_requests_`.
  base::flat_map<mojom::ClientId, std::vector<OptGuideRequest>>
      unsent_opt_guide_requests_;
  // Stores all the Optimization Guide requests that have already been sent, and
  // are awaiting a response from the Optimization Guide service.
  base::flat_map<mojom::ClientId, std::vector<OptGuideRequest>>
      sent_opt_guide_requests_;

  // The timers used to allow for some requests to accumulate before sending a
  // batch request to Optimization Guide Service. One timer per client ID.
  // Insertion doesn't compile unless the timer is wrapped in a unique pointer.
  base::flat_map<mojom::ClientId, std::unique_ptr<base::OneShotTimer>>
      opt_guide_timers_;

  base::WeakPtrFactory<ImageServiceImpl> weak_factory_{this};
};

}  // namespace page_image_service

#endif  // COMPONENTS_PAGE_IMAGE_SERVICE_IMAGE_SERVICE_IMPL_H_
