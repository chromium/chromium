// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/actor/core/origin_checker.h"

#include <algorithm>
#include <variant>

#include "base/containers/map_util.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/core/actor_metrics.h"
#include "components/actor/core/actor_util.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "url/origin.h"

namespace actor {

OriginChecker::OriginChecker()
    : allowed_navigation_origins_(kGlicNavigationGatingUseSiteNotOrigin.Get()
                                      ? StateMap(SiteMap())
                                      : StateMap(OriginMap())) {}
OriginChecker::~OriginChecker() = default;

bool OriginChecker::IsNavigationAllowed(
    const url::Origin& source_origin,
    const url::Origin& destination_origin) const {
  CHECK(IsNavigationGatingEnabled());

  return std::visit(
      absl::Overload{
          [&](const OriginMap& origins) -> bool {
            return source_origin == destination_origin ||
                   origins.contains(destination_origin);
          },
          [&](const SiteMap& sites) -> bool {
            return net::SchemefulSite::IsSameSite(source_origin,
                                                  destination_origin) ||
                   sites.contains(net::SchemefulSite(destination_origin));
          },
      },
      allowed_navigation_origins_);
}

bool OriginChecker::IsNavigationConfirmedByUser(
    const url::Origin& origin) const {
  const auto* state = std::visit(absl::Overload{
                                     [&](const OriginMap& origins) {
                                       return base::FindOrNull(origins, origin);
                                     },
                                     [&](const SiteMap& sites) {
                                       return base::FindOrNull(
                                           sites, net::SchemefulSite(origin));
                                     },
                                 },
                                 allowed_navigation_origins_);
  return state && state->is_user_confirmed;
}

void OriginChecker::AllowNavigationTo(url::Origin origin,
                                      bool is_user_confirmed) {
  auto& state = std::visit(
      absl::Overload{
          [&](OriginMap& origins) -> State& {
            return origins.emplace(origin, State{is_user_confirmed})
                .first->second;
          },
          [&](SiteMap& sites) -> State& {
            return sites
                .emplace(net::SchemefulSite(origin), State{is_user_confirmed})
                .first->second;
          },
      },
      allowed_navigation_origins_);
  if (is_user_confirmed) {
    state.is_user_confirmed = true;
  }
}

void OriginChecker::AllowNavigationTo(
    const absl::flat_hash_set<url::Origin>& origins) {
  std::visit(
      absl::Overload{
          [&](OriginMap& origin_states) {
            std::ranges::transform(
                origins, std::inserter(origin_states, origin_states.end()),
                [](const auto& origin) {
                  return std::make_pair(origin,
                                        State{/*is_user_confirmed=*/false});
                });
          },
          [&](SiteMap& sites) {
            std::ranges::transform(origins, std::inserter(sites, sites.end()),
                                   [](const auto& origin) {
                                     return std::make_pair(
                                         net::SchemefulSite(origin),
                                         State{/*is_user_confirmed=*/false});
                                   });
          },
      },
      allowed_navigation_origins_);
}

void OriginChecker::RecordSizeMetrics() const {
  auto [total, user_confirmed_total] =
      std::visit(absl::Overload{
                     [](const OriginMap& origins) {
                       return std::make_pair(
                           origins.size(),
                           std::ranges::count_if(origins, [](const auto& pair) {
                             return pair.second.is_user_confirmed;
                           }));
                     },
                     [](const SiteMap& sites) {
                       return std::make_pair(
                           sites.size(),
                           std::ranges::count_if(sites, [](const auto& pair) {
                             return pair.second.is_user_confirmed;
                           }));
                     },
                 },
                 allowed_navigation_origins_);
  RecordActorNavigationGatingListSize(total, user_confirmed_total);
}

}  // namespace actor
