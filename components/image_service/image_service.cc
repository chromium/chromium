// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_service/image_service.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/image_service/features.h"
#include "components/image_service/metrics_util.h"
#include "components/omnibox/browser/remote_suggestions_service.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/optimization_guide/core/new_optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/salient_image_metadata.pb.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace image_service {

namespace {

// Fulfills all `callbacks` with `result`.
void FulfillAllCallbacks(std::vector<ImageService::ResultCallback> callbacks,
                         const GURL& result) {
  for (auto& callback : callbacks) {
    std::move(callback).Run(result);
  }
}

}  // namespace

// A one-time use object that uses Suggest to get an image URL corresponding
// to `search_query` and `entity_id`. This is a hacky temporary implementation,
// ideally this should be replaced by persisted Suggest-provided entities.
// TODO(tommycli): Move this to its own separate file with unit tests.
class ImageService::SuggestEntityImageURLFetcher {
 public:
  SuggestEntityImageURLFetcher(
      AutocompleteProviderClient* autocomplete_provider_client,
      const std::u16string& search_query,
      const std::string& entity_id)
      : autocomplete_provider_client_(autocomplete_provider_client),
        search_query_(base::i18n::ToLower(search_query)),
        entity_id_(entity_id) {
    DCHECK(autocomplete_provider_client);
  }
  SuggestEntityImageURLFetcher(const SuggestEntityImageURLFetcher&) = delete;

  // `callback` is called with the result.
  void Start(base::OnceCallback<void(const GURL&)> callback) {
    const TemplateURLService* template_url_service =
        autocomplete_provider_client_->GetTemplateURLService();
    if (template_url_service == nullptr) {
      return std::move(callback).Run(GURL());
    }

    // We are relying on the user's consent to Sync History, which in practice
    // means only Google should get URL-keyed metadata requests via Suggest.
    const TemplateURL* template_url =
        template_url_service->GetDefaultSearchProvider();
    if (template_url == nullptr ||
        template_url->GetEngineType(
            template_url_service->search_terms_data()) !=
            SEARCH_ENGINE_GOOGLE) {
      return std::move(callback).Run(GURL());
    }

    DCHECK(!callback_);
    callback_ = std::move(callback);

    TemplateURLRef::SearchTermsArgs search_terms_args;
    search_terms_args.page_classification =
        metrics::OmniboxEventProto::JOURNEYS;
    search_terms_args.search_terms = search_query_;

    loader_ =
        autocomplete_provider_client_
            ->GetRemoteSuggestionsService(/*create_if_necessary=*/true)
            ->StartSuggestionsRequest(
                template_url, search_terms_args,
                template_url_service->search_terms_data(),
                base::BindOnce(&SuggestEntityImageURLFetcher::OnURLLoadComplete,
                               weak_factory_.GetWeakPtr()));
  }

 private:
  void OnURLLoadComplete(const network::SimpleURLLoader* source,
                         const bool response_received,
                         std::unique_ptr<std::string> response_body) {
    DCHECK_EQ(loader_.get(), source);
    if (!response_received) {
      return std::move(callback_).Run(GURL());
    }

    std::string response_json = SearchSuggestionParser::ExtractJsonData(
        source, std::move(response_body));
    if (response_json.empty()) {
      return std::move(callback_).Run(GURL());
    }

    auto response_data =
        SearchSuggestionParser::DeserializeJsonData(response_json);
    if (!response_data) {
      return std::move(callback_).Run(GURL());
    }

    AutocompleteInput input(
        search_query_, metrics::OmniboxEventProto::JOURNEYS,
        autocomplete_provider_client_->GetSchemeClassifier());
    SearchSuggestionParser::Results results;
    if (!SearchSuggestionParser::ParseSuggestResults(
            *response_data, input,
            autocomplete_provider_client_->GetSchemeClassifier(),
            /*default_result_relevance=*/100,
            /*is_keyword_result=*/false, &results)) {
      return std::move(callback_).Run(GURL());
    }

    for (const auto& result : results.suggest_results) {
      // TODO(tommycli): `entity_id_` is not used yet, because it's always
      // empty right now.
      GURL url(result.entity_info().image_url());
      if (url.is_valid() &&
          base::i18n::ToLower(result.match_contents()) == search_query_) {
        return std::move(callback_).Run(std::move(url));
      }
    }

    // If we didn't find any matching images, still notify the caller.
    if (!callback_.is_null())
      std::move(callback_).Run(GURL());
  }

  const raw_ptr<AutocompleteProviderClient> autocomplete_provider_client_;

  // The search query and entity ID we are searching for.
  const std::u16string search_query_;
  const std::string entity_id_;

  // The result callback to be called once we get the answer.
  base::OnceCallback<void(const GURL&)> callback_;

  // The URL loader used to get the suggestions.
  std::unique_ptr<network::SimpleURLLoader> loader_;

  base::WeakPtrFactory<SuggestEntityImageURLFetcher> weak_factory_{this};
};

ImageService::ImageService(
    std::unique_ptr<AutocompleteProviderClient> autocomplete_provider_client,
    optimization_guide::NewOptimizationGuideDecider* opt_guide,
    syncer::SyncService* sync_service)
    : autocomplete_provider_client_(std::move(autocomplete_provider_client)),
      history_consent_throttle_(
          unified_consent::UrlKeyedDataCollectionConsentHelper::
              NewPersonalizedDataCollectionConsentHelper(sync_service)),
      bookmarks_consent_throttle_(
          unified_consent::UrlKeyedDataCollectionConsentHelper::
              NewPersonalizedBookmarksDataCollectionConsentHelper(
                  sync_service)) {
  if (opt_guide && base::FeatureList::IsEnabled(
                       kImageServiceOptimizationGuideSalientImages)) {
    opt_guide_ = opt_guide;
  }
}

ImageService::OptGuideRequest::OptGuideRequest() = default;
ImageService::OptGuideRequest::~OptGuideRequest() = default;
ImageService::OptGuideRequest::OptGuideRequest(OptGuideRequest&& other) =
    default;

ImageService::~ImageService() = default;

base::WeakPtr<ImageService> ImageService::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void ImageService::FetchImageFor(mojom::ClientId client_id,
                                 const GURL& page_url,
                                 const mojom::Options& options,
                                 ResultCallback callback) {
  if (!base::FeatureList::IsEnabled(kImageService)) {
    // In general this should never happen, because each UI should have its own
    // feature gate, but this is just so we have a whole-service killswitch.
    return std::move(callback).Run(GURL());
  }

  GetConsentToFetchImage(
      client_id,
      base::BindOnce(&ImageService::OnConsentResult, weak_factory_.GetWeakPtr(),
                     client_id, page_url, options, std::move(callback)));
}

void ImageService::GetConsentToFetchImage(
    mojom::ClientId client_id,
    base::OnceCallback<void(bool)> callback) {
  switch (client_id) {
    case mojom::ClientId::Journeys:
    case mojom::ClientId::JourneysSidePanel:
    case mojom::ClientId::NtpQuests: {
      return history_consent_throttle_.EnqueueRequest(std::move(callback));
    }
    case mojom::ClientId::NtpRealbox:
      // TODO(b/244507194): Figure out consent story for NTP realbox case.
      return std::move(callback).Run(false);
    case mojom::ClientId::Bookmarks: {
      return bookmarks_consent_throttle_.EnqueueRequest(std::move(callback));
    }
  }
}

void ImageService::OnConsentResult(mojom::ClientId client_id,
                                   const GURL& page_url,
                                   const mojom::Options& options,
                                   ResultCallback callback,
                                   bool consent_is_enabled) {
  if (!consent_is_enabled) {
    return std::move(callback).Run(GURL());
  }

  if (options.suggest_images &&
      base::FeatureList::IsEnabled(kImageServiceSuggestPoweredImages)) {
    // TODO(b/244507194): Get our "own" TemplateURLService.
    if (auto* template_url_service =
            autocomplete_provider_client_->GetTemplateURLService()) {
      auto search_metadata =
          template_url_service->ExtractSearchMetadata(page_url);
      // Fetch entity-keyed images for Google SRP visits only, because only
      // Google SRP visits can expect to have a reasonable entity from Google
      // Suggest.
      if (search_metadata && search_metadata->template_url &&
          search_metadata->template_url->GetEngineType(
              template_url_service->search_terms_data()) ==
              SEARCH_ENGINE_GOOGLE) {
        UmaHistogramEnumerationForClient(kBackendHistogramName,
                                         PageImageServiceBackend::kSuggest,
                                         client_id);
        return FetchSuggestImage(/*search_query=*/search_metadata->search_terms,
                                 /*entity_id=*/"", std::move(callback));
      }
    }
  }

  if (options.optimization_guide_images && opt_guide_ &&
      base::FeatureList::IsEnabled(
          kImageServiceOptimizationGuideSalientImages)) {
    UmaHistogramEnumerationForClient(
        kBackendHistogramName, PageImageServiceBackend::kOptimizationGuide,
        client_id);
    return FetchOptimizationGuideImage(client_id, page_url,
                                       std::move(callback));
  }

  UmaHistogramEnumerationForClient(kBackendHistogramName,
                                   PageImageServiceBackend::kNoValidBackend,
                                   client_id);
  std::move(callback).Run(GURL());
}

void ImageService::FetchSuggestImage(const std::u16string& search_query,
                                     const std::string& entity_id,
                                     ResultCallback callback) {
  auto fetcher = std::make_unique<SuggestEntityImageURLFetcher>(
      autocomplete_provider_client_.get(), search_query, entity_id);

  // Use a raw pointer temporary so we can give ownership of the unique_ptr to
  // the callback and have a well defined SuggestEntityImageURLFetcher lifetime.
  auto* fetcher_raw_ptr = fetcher.get();
  fetcher_raw_ptr->Start(base::BindOnce(
      &ImageService::OnSuggestImageFetched, weak_factory_.GetWeakPtr(),
      std::move(fetcher), std::move(callback)));
}

void ImageService::OnSuggestImageFetched(
    std::unique_ptr<SuggestEntityImageURLFetcher> fetcher,
    ResultCallback callback,
    const GURL& image_url) {
  std::move(callback).Run(image_url);

  // `fetcher` is owned by this method and will be deleted now.
}

void ImageService::FetchOptimizationGuideImage(mojom::ClientId client_id,
                                               const GURL& page_url,
                                               ResultCallback callback) {
  DCHECK(opt_guide_) << "FetchOptimizationGuideImage is never called when "
                        "opt_guide_ is nullptr.";

  OptGuideRequest request;
  request.url = page_url;
  request.callback = std::move(callback);
  auto& request_list = unsent_opt_guide_requests_[client_id];
  request_list.push_back(std::move(request));

  if (request_list.size() >=
      optimization_guide::features::
          MaxUrlsForOptimizationGuideServiceHintsFetch()) {
    // Erasing the timer also cancels the timer callback.
    opt_guide_timers_.erase(client_id);
    ProcessAllBatchedOptimizationGuideRequests(client_id);
  } else if (request_list.size() == 1U) {
    // Otherwise, if we just enqueued our FIRST request, then kick off a timer
    // to flush the queue. One millisecond is a long enough time in CPU time.
    auto timer = std::make_unique<base::OneShotTimer>();
    timer->Start(FROM_HERE, kOptimizationGuideBatchingTimeout,
                 base::BindOnce(
                     &ImageService::ProcessAllBatchedOptimizationGuideRequests,
                     weak_factory_.GetWeakPtr(), client_id));
    opt_guide_timers_[client_id] = std::move(timer);
  }
}

void ImageService::ProcessAllBatchedOptimizationGuideRequests(
    mojom::ClientId client_id) {
  optimization_guide::proto::RequestContext request_context;
  switch (client_id) {
    case mojom::ClientId::Journeys:
    case mojom::ClientId::JourneysSidePanel: {
      request_context = optimization_guide::proto::CONTEXT_JOURNEYS;
      break;
    }
    case mojom::ClientId::NtpQuests:
    case mojom::ClientId::NtpRealbox: {
      request_context = optimization_guide::proto::CONTEXT_NEW_TAB_PAGE;
      break;
    }
    case mojom::ClientId::Bookmarks: {
      request_context = optimization_guide::proto::CONTEXT_BOOKMARKS;
      break;
    }
  }

  std::vector<OptGuideRequest>& unsent_requests =
      unsent_opt_guide_requests_[client_id];
  if (unsent_requests.empty()) {
    return;
  }

  // Generate a list of URLs to request in this batch.
  std::vector<GURL> urls;
  for (auto& request : unsent_requests) {
    urls.push_back(request.url);
  }

  // Move the list of unsent requests to the sent vector.
  for (auto& request : unsent_requests) {
    sent_opt_guide_requests_[client_id].push_back(std::move(request));
  }
  unsent_requests.clear();

  opt_guide_->CanApplyOptimizationOnDemand(
      urls, {optimization_guide::proto::OptimizationType::SALIENT_IMAGE},
      request_context,
      base::BindRepeating(&ImageService::OnOptimizationGuideImageFetched,
                          weak_factory_.GetWeakPtr(), client_id));
}

void ImageService::OnOptimizationGuideImageFetched(
    mojom::ClientId client_id,
    const GURL& url,
    const base::flat_map<
        optimization_guide::proto::OptimizationType,
        optimization_guide::OptimizationGuideDecisionWithMetadata>& decisions) {
  // Extract all waiting callbacks matching `url` to `matching_callbacks`.
  std::vector<ResultCallback> matching_callbacks;
  {
    // Take over the existing whole list via a swap.
    std::vector<OptGuideRequest> all_requests;
    std::swap(all_requests, sent_opt_guide_requests_[client_id]);

    // Steal the matching callbacks, pushing back the other pending requests
    // back to the original list.
    for (auto& request : all_requests) {
      if (request.url == url) {
        matching_callbacks.push_back(std::move(request.callback));
      } else {
        sent_opt_guide_requests_[client_id].push_back(std::move(request));
      }
    }
  }

  auto iter = decisions.find(optimization_guide::proto::SALIENT_IMAGE);
  if (iter == decisions.end()) {
    UmaHistogramEnumerationForClient(
        kBackendOptimizationGuideResultHistogramName,
        PageImageServiceOptimizationGuideResult::kDecisionMissing, client_id);
    return FulfillAllCallbacks(std::move(matching_callbacks), GURL());
  }

  optimization_guide::OptimizationGuideDecisionWithMetadata decision =
      iter->second;
  if (decision.decision !=
      optimization_guide::OptimizationGuideDecision::kTrue) {
    UmaHistogramEnumerationForClient(
        kBackendOptimizationGuideResultHistogramName,
        PageImageServiceOptimizationGuideResult::kNoImage, client_id);
    return FulfillAllCallbacks(std::move(matching_callbacks), GURL());
  }
  if (!decision.metadata.any_metadata().has_value()) {
    UmaHistogramEnumerationForClient(
        kBackendOptimizationGuideResultHistogramName,
        PageImageServiceOptimizationGuideResult::kResponseMalformed, client_id);
    return FulfillAllCallbacks(std::move(matching_callbacks), GURL());
  }

  auto parsed_any = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::SalientImageMetadata>(
      decision.metadata.any_metadata().value());
  if (!parsed_any) {
    UmaHistogramEnumerationForClient(
        kBackendOptimizationGuideResultHistogramName,
        PageImageServiceOptimizationGuideResult::kResponseMalformed, client_id);
    return FulfillAllCallbacks(std::move(matching_callbacks), GURL());
  }

  // Look through the metadata, returning the first valid image URL.
  auto salient_image_metadata = *parsed_any;
  for (const auto& thumbnail : salient_image_metadata.thumbnails()) {
    if (thumbnail.has_image_url()) {
      GURL image_url(thumbnail.image_url());
      if (image_url.is_valid()) {
        UmaHistogramEnumerationForClient(
            kBackendOptimizationGuideResultHistogramName,
            PageImageServiceOptimizationGuideResult::kSuccess, client_id);
        return FulfillAllCallbacks(std::move(matching_callbacks), image_url);
      }
    }
  }

  // Fail if we can't find any.
  UmaHistogramEnumerationForClient(
      kBackendOptimizationGuideResultHistogramName,
      PageImageServiceOptimizationGuideResult::kResponseMalformed, client_id);
  return FulfillAllCallbacks(std::move(matching_callbacks), GURL());
}

}  // namespace image_service
