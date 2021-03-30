// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTINUOUS_SEARCH_BROWSER_SEARCH_RESULT_EXTRACTOR_CLIENT_H_
#define COMPONENTS_CONTINUOUS_SEARCH_BROWSER_SEARCH_RESULT_EXTRACTOR_CLIENT_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/continuous_search/browser/search_result_extractor_client_status.h"
#include "components/continuous_search/common/public/mojom/continuous_search.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace continuous_search {

// A client of the `mojom::SearchResultExtractor` interface.
class SearchResultExtractorClient {
 public:
  SearchResultExtractorClient();
  ~SearchResultExtractorClient();

  SearchResultExtractorClient(const SearchResultExtractorClient&) = delete;
  SearchResultExtractorClient& operator=(const SearchResultExtractorClient&) =
      delete;

  using RequestDataCallback =
      base::OnceCallback<void(SearchResultExtractorClientStatus,
                              mojom::CategoryResultsPtr)>;

  // Requests extraction of SRP data from the main frame of `web_contents`.
  // Results are returned to `callback`.
  void RequestData(content::WebContents* web_contents,
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

  base::WeakPtrFactory<SearchResultExtractorClient> weak_ptr_factory_{this};
};

}  // namespace continuous_search

#endif  // COMPONENTS_CONTINUOUS_SEARCH_BROWSER_SEARCH_RESULT_EXTRACTOR_CLIENT_H_
