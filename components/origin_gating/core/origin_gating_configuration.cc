// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/origin_gating_configuration.h"

#include <algorithm>
#include <utility>

#include "base/check.h"

namespace origin_gating {

namespace {

constexpr DecisionSource kForbiddenPredicates[] = {
    DecisionSource::kNoVerdict,
};

}  // namespace

OriginGatingConfiguration::OriginGatingConfiguration(
    std::initializer_list<DecisionSource> predicates,
    bool use_site_keyed_cache)
    : predicates_(predicates), use_site_keyed_cache_(use_site_keyed_cache) {
  CHECK(std::ranges::none_of(predicates, [](DecisionSource p) {
    return std::ranges::contains(kForbiddenPredicates, p);
  }));
}

OriginGatingConfiguration::~OriginGatingConfiguration() = default;

OriginGatingConfiguration::OriginGatingConfiguration(
    const OriginGatingConfiguration&) = default;

OriginGatingConfiguration& OriginGatingConfiguration::operator=(
    const OriginGatingConfiguration&) = default;

}  // namespace origin_gating
