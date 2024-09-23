// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/affiliation_fetcher_delegate.h"

namespace affiliations {

AffiliationFetcherDelegate::Result::Result() = default;

AffiliationFetcherDelegate::Result::Result(const Result& other) = default;

AffiliationFetcherDelegate::Result::Result(Result&& other) = default;

AffiliationFetcherDelegate::Result&
AffiliationFetcherDelegate::Result::operator=(const Result& other) = default;

AffiliationFetcherDelegate::Result&
AffiliationFetcherDelegate::Result::operator=(Result&& other) = default;

AffiliationFetcherDelegate::Result::~Result() = default;

}  // namespace affiliations
