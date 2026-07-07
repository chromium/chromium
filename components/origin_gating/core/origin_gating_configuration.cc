// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/origin_gating_configuration.h"

#include <algorithm>
#include <initializer_list>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"

namespace origin_gating {

namespace {

constexpr DecisionSource kForbiddenPredicates[] = {
    DecisionSource::kNoVerdict,
};

}  // namespace

CustomPredicate::CustomPredicate(AsyncPredicate predicate,
                                 std::string_view name)
    : predicate_(std::move(predicate)), name_(name) {}

CustomPredicate::CustomPredicate(SyncPredicate predicate, std::string_view name)
    : CustomPredicate(base::BindRepeating(
                          [](SyncPredicate sync_pred,
                             const GatingDecisionContext* context,
                             const GURL& source,
                             const GURL& destination,
                             base::OnceCallback<void(Decision)> callback) {
                            std::move(callback).Run(
                                sync_pred.Run(context, source, destination));
                          },
                          std::move(predicate)),
                      name) {}

CustomPredicate::~CustomPredicate() = default;

CustomPredicate::CustomPredicate(const CustomPredicate&) = default;

CustomPredicate& CustomPredicate::operator=(const CustomPredicate&) = default;

void CustomPredicate::Run(const GatingDecisionContext* context,
                          const GURL& source,
                          const GURL& destination,
                          base::OnceCallback<void(Decision)> callback) const {
  predicate_.Run(context, source, destination, std::move(callback));
}

OriginGatingConfiguration::OriginGatingConfiguration(
    std::initializer_list<Predicate> predicates,
    bool use_site_keyed_cache)
    : predicates_(predicates), use_site_keyed_cache_(use_site_keyed_cache) {
  CHECK(std::ranges::none_of(predicates, [](const Predicate& p) {
    const DecisionSource* source = std::get_if<DecisionSource>(&p);
    return source && std::ranges::contains(kForbiddenPredicates, *source);
  }));
}

OriginGatingConfiguration::~OriginGatingConfiguration() = default;

OriginGatingConfiguration::OriginGatingConfiguration(
    const OriginGatingConfiguration&) = default;

OriginGatingConfiguration& OriginGatingConfiguration::operator=(
    const OriginGatingConfiguration&) = default;

}  // namespace origin_gating
