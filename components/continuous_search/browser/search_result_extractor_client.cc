// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/continuous_search/browser/search_result_extractor_client.h"

#include "base/strings/utf_string_conversions.h"
#include "components/continuous_search/common/title_validator.h"
#include "components/google/core/common/google_util.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace continuous_search {

namespace {

SearchResultExtractorClientStatus ToSearchResultExtractorClientStatus(
    mojom::SearchResultExtractor::Status status) {
  switch (status) {
    case mojom::SearchResultExtractor::Status::kSuccess:
      return SearchResultExtractorClientStatus::kSuccess;
    case mojom::SearchResultExtractor::Status::kNoResults:
      return SearchResultExtractorClientStatus::kNoResults;
  }
}

}  // namespace

SearchResultExtractorClient::SearchResultExtractorClient(bool test_mode)
    : test_mode_(test_mode) {}
SearchResultExtractorClient::~SearchResultExtractorClient() = default;

void SearchResultExtractorClient::RequestData(
    content::WebContents* web_contents,
    const std::vector<mojom::ResultType>& result_types,
    RequestDataCallback callback) {
  if (!web_contents || !web_contents->GetPrimaryMainFrame() ||
      !web_contents->GetPrimaryMainFrame()->GetRemoteAssociatedInterfaces()) {
    std::move(callback).Run(SearchResultExtractorClientStatus::kWebContentsGone,
                            mojom::CategoryResults::New());
    return;
  }

  const GURL& url = web_contents->GetLastCommittedURL();
  if (!google_util::IsGoogleSearchUrl(url) && !test_mode_) {
    std::move(callback).Run(
        SearchResultExtractorClientStatus::kWebContentsHasNonSrpUrl,
        mojom::CategoryResults::New());
    return;
  }

  mojo::AssociatedRemote<mojom::SearchResultExtractor> extractor;
  web_contents->GetPrimaryMainFrame()
      ->GetRemoteAssociatedInterfaces()
      ->GetInterface(extractor.BindNewEndpointAndPassReceiver());

  mojom::SearchResultExtractor* extractor_ptr = extractor.get();
  extractor_ptr->ExtractCurrentSearchResults(
      result_types,
      base::BindOnce(&SearchResultExtractorClient::RequestDataCallbackAdapter,
                     weak_ptr_factory_.GetWeakPtr(), std::move(extractor), url,
                     std::move(callback)));
}

void SearchResultExtractorClient::RequestDataCallbackAdapter(
    mojo::AssociatedRemote<mojom::SearchResultExtractor> extractor,
    const GURL& url,
    RequestDataCallback callback,
    mojom::SearchResultExtractor::Status status,
    mojom::CategoryResultsPtr results) {
  if (status != mojom::SearchResultExtractor::Status::kSuccess) {
    std::move(callback).Run(ToSearchResultExtractorClientStatus(status),
                            mojom::CategoryResults::New());
    return;
  }

  // Ensure the URL requested is the URL returned with the data.
  if (url != results->document_url) {
    std::move(callback).Run(SearchResultExtractorClientStatus::kUnexpectedUrl,
                            mojom::CategoryResults::New());
    return;
  }

  // Validate all the returned titles (URL already needs to be valid for mojom).
  for (mojom::ResultGroupPtr& group : results->groups) {
    for (mojom::SearchResultPtr& result : group->results) {
      result->title = ValidateTitle(result->title);
    }
  }

  // `url` and transitively `document_url` should always be a search URL. If
  // this is not the case an invariant has been violated.
  CHECK(google_util::IsGoogleSearchUrl(results->document_url) || test_mode_);
  std::move(callback).Run(SearchResultExtractorClientStatus::kSuccess,
                          std::move(results));
}

}  // namespace continuous_search
