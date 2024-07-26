// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_deduplication/deduplication_strategy.h"

namespace url_deduplication {

DeduplicationStrategy::DeduplicationStrategy() = default;

DeduplicationStrategy::DeduplicationStrategy(
    const DeduplicationStrategy& strategy) = default;

DeduplicationStrategy::~DeduplicationStrategy() = default;

}  // namespace url_deduplication
