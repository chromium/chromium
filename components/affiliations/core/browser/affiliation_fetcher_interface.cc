// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/affiliation_fetcher_interface.h"

namespace affiliations {
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
}  // namespace affiliations
