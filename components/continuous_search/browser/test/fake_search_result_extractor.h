// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTINUOUS_SEARCH_BROWSER_TEST_FAKE_SEARCH_RESULT_EXTRACTOR_H_
#define COMPONENTS_CONTINUOUS_SEARCH_BROWSER_TEST_FAKE_SEARCH_RESULT_EXTRACTOR_H_

#include <vector>

#include "components/continuous_search/common/public/mojom/continuous_search.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"

namespace continuous_search {

class FakeSearchResultExtractor : public mojom::SearchResultExtractor {
 public:
  FakeSearchResultExtractor();
  ~FakeSearchResultExtractor() override;

  FakeSearchResultExtractor(const FakeSearchResultExtractor&) = delete;
  FakeSearchResultExtractor& operator=(const FakeSearchResultExtractor&) =
      delete;

  void ExtractCurrentSearchResults(
      const std::vector<mojom::ResultType>& result_types,
      ExtractCurrentSearchResultsCallback callback) override;

  // Sets the `status` and `results` returned the next time
  // `ExtractCurrentSearchResults()` is invoked. Must be invoked between each
  // call `ExtractCurrentSearchResults()` to properly update `results`.
  void SetResponse(mojom::SearchResultExtractor::Status status,
                   mojom::CategoryResultsPtr results);

  void BindRequest(mojo::ScopedInterfaceEndpointHandle handle);

 private:
  bool response_set_{false};
  mojom::SearchResultExtractor::Status status_;
  mojom::CategoryResultsPtr results_;
  mojo::AssociatedReceiver<mojom::SearchResultExtractor> binding_{this};
};

}  // namespace continuous_search

#endif  // COMPONENTS_CONTINUOUS_SEARCH_BROWSER_TEST_FAKE_SEARCH_RESULT_EXTRACTOR_H_
