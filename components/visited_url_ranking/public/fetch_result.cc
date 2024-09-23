// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/public/fetch_result.h"

#include <map>
#include <string>

#include "components/visited_url_ranking/public/url_visit.h"

namespace visited_url_ranking {

FetchResult::FetchResult(Status status_arg,
                         std::map<URLMergeKey, URLVisitVariant> data_arg)
    : status(status_arg), data(std::move(data_arg)) {}

FetchResult::~FetchResult() = default;

FetchResult::FetchResult(FetchResult&& other) = default;

FetchResult& FetchResult::operator=(FetchResult&& other) = default;

}  // namespace visited_url_ranking
