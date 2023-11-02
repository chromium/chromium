// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/continuous_search/browser/test/fake_search_result_extractor.h"

#include <utility>

namespace continuous_search {

FakeSearchResultExtractor::FakeSearchResultExtractor() = default;
FakeSearchResultExtractor::~FakeSearchResultExtractor() = default;

void FakeSearchResultExtractor::ExtractCurrentSearchResults(
    const std::vector<mojom::ResultType>& result_types,
    ExtractCurrentSearchResultsCallback callback) {
  CHECK(response_set_);
  std::move(callback).Run(status_, std::move(results_));
  response_set_ = false;
}

void FakeSearchResultExtractor::SetResponse(
    mojom::SearchResultExtractor::Status status,
    mojom::CategoryResultsPtr results) {
  status_ = status;
  results_ = std::move(results);
  response_set_ = true;
}

void FakeSearchResultExtractor::BindRequest(
    mojo::ScopedInterfaceEndpointHandle handle) {
  binding_.Bind(mojo::PendingAssociatedReceiver<mojom::SearchResultExtractor>(
      std::move(handle)));
}

}  // namespace continuous_search
