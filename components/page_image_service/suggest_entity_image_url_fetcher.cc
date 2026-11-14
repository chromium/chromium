// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_image_service/suggest_entity_image_url_fetcher.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_scheme_classifier.h"
#include "components/omnibox/browser/remote_suggestions_service.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/page_image_service/metrics_util.h"
#include "components/search_engines/template_url.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/omnibox_proto/suggest_template_info.pb.h"
#include "url/gurl.h"

namespace page_image_service {

SuggestEntityImageURLFetcher::SuggestEntityImageURLFetcher(
    const AutocompleteSchemeClassifier& autocomplete_scheme_classifier,
    mojom::ClientId client_id,
    const std::u16string& search_query,
    const std::string& entity_id)
    : autocomplete_scheme_classifier_(autocomplete_scheme_classifier),
      client_id_(client_id),
      search_query_(base::i18n::ToLower(search_query)),
      entity_id_(entity_id) {}

SuggestEntityImageURLFetcher::~SuggestEntityImageURLFetcher() = default;

void SuggestEntityImageURLFetcher::Start(
    const TemplateURL* template_url,
    const SearchTermsData& search_terms_data,
    RemoteSuggestionsService* remote_suggestions_service,
    base::OnceCallback<void(const GURL&)> callback) {
  CHECK(template_url);
  CHECK(remote_suggestions_service);
  CHECK(!callback_);
  callback_ = std::move(callback);

  TemplateURLRef::SearchTermsArgs search_terms_args;
  search_terms_args.page_classification = metrics::OmniboxEventProto::JOURNEYS;
  search_terms_args.search_terms = search_query_;

  // ImageServiceFactory does not create a service instance for OTR profiles.
  loader_ = remote_suggestions_service->StartSuggestionsRequest(
      RemoteRequestType::kImages, /*is_off_the_record=*/false, template_url,
      search_terms_args, search_terms_data,
      base::BindOnce(&SuggestEntityImageURLFetcher::OnURLLoadComplete,
                     weak_factory_.GetWeakPtr()));

  // No request is started if `template_url` has no valid suggestions URL.
  // Still notify the caller. Post the callback, because it owns this fetcher
  // (see ImageServiceImpl::FetchSuggestImage()).
  if (!loader_) {
    UmaHistogramEnumerationForClient(kBackendSuggestResultHistogramName,
                                     PageImageServiceResult::kResponseMissing,
                                     client_id_);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), GURL()));
  }
}

void SuggestEntityImageURLFetcher::OnURLLoadComplete(
    const network::SimpleURLLoader* source,
    const int response_code,
    std::optional<std::string> response_body) {
  DCHECK_EQ(loader_.get(), source);
  if (response_code != 200) {
    UmaHistogramEnumerationForClient(kBackendSuggestResultHistogramName,
                                     PageImageServiceResult::kResponseMissing,
                                     client_id_);
    return std::move(callback_).Run(GURL());
  }

  std::string response_json =
      SearchSuggestionParser::ExtractJsonData(source, std::move(response_body));
  if (response_json.empty()) {
    UmaHistogramEnumerationForClient(kBackendSuggestResultHistogramName,
                                     PageImageServiceResult::kResponseMalformed,
                                     client_id_);
    return std::move(callback_).Run(GURL());
  }

  auto response_data =
      SearchSuggestionParser::DeserializeJsonData(response_json);
  if (!response_data) {
    UmaHistogramEnumerationForClient(kBackendSuggestResultHistogramName,
                                     PageImageServiceResult::kResponseMalformed,
                                     client_id_);
    return std::move(callback_).Run(GURL());
  }

  AutocompleteInput input(search_query_, metrics::OmniboxEventProto::JOURNEYS,
                          *autocomplete_scheme_classifier_);
  SearchSuggestionParser::Results results;
  if (!SearchSuggestionParser::ParseSuggestResults(
          *response_data, input, *autocomplete_scheme_classifier_,
          /*default_result_relevance=*/100,
          /*is_keyword_result=*/false, &results)) {
    UmaHistogramEnumerationForClient(kBackendSuggestResultHistogramName,
                                     PageImageServiceResult::kResponseMalformed,
                                     client_id_);
    return std::move(callback_).Run(GURL());
  }

  for (const auto& result : results.suggest_results) {
    // TODO(tommycli): `entity_id_` is not used yet, because it's always
    // empty right now.
    GURL url(result.suggest_template_info()
                 ? result.suggest_template_info()->image().url()
                 : "");
    if (url.is_valid() &&
        base::i18n::ToLower(result.match_contents()) == search_query_) {
      UmaHistogramEnumerationForClient(kBackendSuggestResultHistogramName,
                                       PageImageServiceResult::kSuccess,
                                       client_id_);
      return std::move(callback_).Run(std::move(url));
    }
  }

  // If we didn't find any matching images, still notify the caller.
  if (!callback_.is_null()) {
    UmaHistogramEnumerationForClient(kBackendSuggestResultHistogramName,
                                     PageImageServiceResult::kNoImage,
                                     client_id_);
    std::move(callback_).Run(GURL());
  }
}

}  // namespace page_image_service
