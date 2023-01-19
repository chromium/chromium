// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_service/image_service.h"

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/omnibox/browser/remote_suggestions_service.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/search_engines/template_url.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace image_service {

namespace {

// A one-time use object that encapsulates tagging a vector of clusters with
// entity images. Used to manage all the fetch jobs dispatched, and runs the
// main callback after it's done.
// TODO(tommycli): This is kind of janky and surely not what we want to do.
// Replace this with a dedicated server-side service.
class FetchJobManager {
 public:
  using ResultCallback =
      base::OnceCallback<void(std::vector<history::Cluster>)>;

  struct Request {
    std::u16string query;
    std::string entity_id;
    raw_ptr<history::ClusterVisit> visit;
  };

  explicit FetchJobManager(std::vector<history::Cluster>&& clusters)
      : clusters_(clusters) {}

  void Start(ImageService* service, ResultCallback callback) {
    std::vector<Request> requests;
    for (auto& cluster : clusters_) {
      for (auto& visit : cluster.visits) {
        // Only fetch URL-keyed metadata for visits known to sync.
        if (!visit.annotated_visit.visit_row.is_known_to_sync) {
          continue;
        }

        // Only tag search visits for now.
        const auto& search_terms =
            visit.annotated_visit.content_annotations.search_terms;
        if (!search_terms.empty()) {
          // TODO(tommycli): Add entity_id once implemented.
          requests.push_back({search_terms, "", &visit});
        }
      }
    }

    // If no requests needed, just early exit and give back the clusters.
    if (requests.empty()) {
      return FinishJob(std::move(callback));
    }

    // This encapsulates the final callback and is called after all the requests
    // are completed.
    auto finish_callback = base::BarrierClosure(
        requests.size(),
        base::BindOnce(&FetchJobManager::FinishJob, weak_factory_.GetWeakPtr(),
                       std::move(callback)));

    for (auto& request : requests) {
      service->FetchImageFor(
          request.query, request.entity_id,
          base::BindOnce(&FetchJobManager::OnImageFetchedForVisit,
                         weak_factory_.GetWeakPtr(), request.visit)
              .Then(finish_callback));
    }
  }

 private:
  // Populates the cluster visit's field. This is a member method and not a
  // free function, because `visit` points to memory owned by this object.
  void OnImageFetchedForVisit(history::ClusterVisit* visit,
                              const GURL& image_url) {
    visit->image_url = image_url;
  }

  void FinishJob(ResultCallback callback) {
    std::move(callback).Run(std::move(clusters_));
  }

  std::vector<history::Cluster> clusters_;
  base::WeakPtrFactory<FetchJobManager> weak_factory_{this};
};

// An anonymous function whose only job is to scope the lifetime of
// ClusterVectorImageTaggingJob, then call `callback` with `clusters`.
void DeleteManagerAndRunCallback(
    std::unique_ptr<FetchJobManager> job,
    base::OnceCallback<void(std::vector<history::Cluster>)> callback,
    std::vector<history::Cluster> clusters) {
  std::move(callback).Run(clusters);
}

}  // namespace

// A one-time use object that uses Suggest to get an image URL corresponding
// to `search_query` and `entity_id`. This is a hacky temporary implementation,
// ideally this should be replaced by persisted Suggest-provided entities.
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
                search_terms_args,
                autocomplete_provider_client_->GetTemplateURLService(),
                base::BindOnce(&SuggestEntityImageURLFetcher::OnURLLoadComplete,
                               weak_factory_.GetWeakPtr()));
  }

 private:
  void OnURLLoadComplete(const network::SimpleURLLoader* source,
                         std::unique_ptr<std::string> response_body) {
    DCHECK_EQ(loader_.get(), source);
    const bool response_received =
        response_body && source->NetError() == net::OK &&
        (source->ResponseInfo() && source->ResponseInfo()->headers &&
         source->ResponseInfo()->headers->response_code() == 200);
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
    syncer::SyncService* sync_service)
    : autocomplete_provider_client_(std::move(autocomplete_provider_client)),
      url_consent_helper_(
          unified_consent::UrlKeyedDataCollectionConsentHelper::
              NewPersonalizedDataCollectionConsentHelper(sync_service)) {}

ImageService::~ImageService() = default;

base::WeakPtr<ImageService> ImageService::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void ImageService::PopulateEntityImagesFor(
    std::vector<history::Cluster> clusters,
    base::OnceCallback<void(std::vector<history::Cluster>)> callback) {
  if (!url_consent_helper_ || !url_consent_helper_->IsEnabled()) {
    return std::move(callback).Run(std::move(clusters));
  }

  auto manager = std::make_unique<FetchJobManager>(std::move(clusters));
  // Use a raw pointer temporary so we can give ownership of the unique_ptr to
  // the callback and have a well defined object lifetime.
  auto* manager_ptr = manager.get();
  manager_ptr->Start(
      this, base::BindOnce(&DeleteManagerAndRunCallback, std::move(manager),
                           std::move(callback)));
}

bool ImageService::FetchImageFor(const std::u16string& search_query,
                                 const std::string& entity_id,
                                 ResultCallback callback) {
  DCHECK(url_consent_helper_ && url_consent_helper_->IsEnabled());

  auto fetcher = std::make_unique<SuggestEntityImageURLFetcher>(
      autocomplete_provider_client_.get(), search_query, entity_id);

  // Use a raw pointer temporary so we can give ownership of the unique_ptr to
  // the callback and have a well defined SuggestEntityImageURLFetcher lifetime.
  auto* fetcher_raw_ptr = fetcher.get();
  fetcher_raw_ptr->Start(
      base::BindOnce(&ImageService::OnImageFetched, weak_factory_.GetWeakPtr(),
                     std::move(fetcher), std::move(callback)));
  return true;
}

void ImageService::OnImageFetched(
    std::unique_ptr<SuggestEntityImageURLFetcher> fetcher,
    ResultCallback callback,
    const GURL& image_url) {
  std::move(callback).Run(image_url);

  // `fetcher` is owned by this method and will be deleted now.
}

}  // namespace image_service
