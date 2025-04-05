// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/affiliation_fetcher_interface.h"

namespace affiliations {
// FetchResult
AffiliationFetcherInterface::FetchResult::FetchResult() = default;

AffiliationFetcherInterface::FetchResult::FetchResult(
    const FetchResult& other) = default;

AffiliationFetcherInterface::FetchResult::FetchResult(FetchResult&& other) =
    default;

AffiliationFetcherInterface::FetchResult&
AffiliationFetcherInterface::FetchResult::operator=(const FetchResult& other) =
    default;

AffiliationFetcherInterface::FetchResult&
AffiliationFetcherInterface::FetchResult::operator=(FetchResult&& other) =
    default;

AffiliationFetcherInterface::FetchResult::~FetchResult() = default;

bool AffiliationFetcherInterface::FetchResult::IsSuccessful() const {
  return http_status_code == net::HTTP_OK && data;
}

// ParsedFetchResponse
AffiliationFetcherInterface::ParsedFetchResponse::ParsedFetchResponse() =
    default;

AffiliationFetcherInterface::ParsedFetchResponse::ParsedFetchResponse(
    const ParsedFetchResponse& other) = default;

AffiliationFetcherInterface::ParsedFetchResponse::ParsedFetchResponse(
    ParsedFetchResponse&& other) = default;

AffiliationFetcherInterface::ParsedFetchResponse&
AffiliationFetcherInterface::ParsedFetchResponse::operator=(
    const ParsedFetchResponse& other) = default;

AffiliationFetcherInterface::ParsedFetchResponse&
AffiliationFetcherInterface::ParsedFetchResponse::operator=(
    ParsedFetchResponse&& other) = default;

AffiliationFetcherInterface::ParsedFetchResponse::~ParsedFetchResponse() =
    default;

}  // namespace affiliations
