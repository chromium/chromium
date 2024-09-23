// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTINUOUS_SEARCH_BROWSER_SEARCH_RESULT_EXTRACTOR_CLIENT_H_
#define COMPONENTS_CONTINUOUS_SEARCH_BROWSER_SEARCH_RESULT_EXTRACTOR_CLIENT_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/continuous_search/common/public/mojom/continuous_search.mojom.h"
#include "components/continuous_search/common/search_result_extractor_client_status.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace continuous_search {

// A client of the `mojom::SearchResultExtractor` interface.
class SearchResultExtractorClient {
 public:
  // Whether to start in test mode which skips some validation checks for
  // testing with non-SRP urls. DO NOT SET OUTSIDE TESTS.
  explicit SearchResultExtractorClient(bool test_mode = false);
  ~SearchResultExtractorClient();

  SearchResultExtractorClient(const SearchResultExtractorClient&) = delete;
  SearchResultExtractorClient& operator=(const SearchResultExtractorClient&) =
      delete;

  using RequestDataCallback =
      base::OnceCallback<void(SearchResultExtractorClientStatus,
                              mojom::CategoryResultsPtr)>;

  // Requests extraction of SRP data from the main frame of `web_contents`.
  // Results are returned to `callback`. `result_types` is list of result types
  // to extract. The extraction will fail and no results will be generated if
  // any of the types (except mojom::ResultType::kAds) cannot be extracted.
  void RequestData(content::WebContents* web_contents,
                   const std::vector<mojom::ResultType>& result_types,
                   RequestDataCallback callback);

 private:
  // Adapter for the callback passed to `RequestData()` that handles additional
  // validation of the data. `extractor` is passed along so it lives as long as
  // the request that is in flight.
  void RequestDataCallbackAdapter(
      mojo::AssociatedRemote<mojom::SearchResultExtractor> extractor,
      const GURL& url,
      RequestDataCallback callback,
      mojom::SearchResultExtractor::Status,
      mojom::CategoryResultsPtr results);

  bool test_mode_{false};
  base::WeakPtrFactory<SearchResultExtractorClient> weak_ptr_factory_{this};
};

}  // namespace continuous_search

#endif  // COMPONENTS_CONTINUOUS_SEARCH_BROWSER_SEARCH_RESULT_EXTRACTOR_CLIENT_H_
