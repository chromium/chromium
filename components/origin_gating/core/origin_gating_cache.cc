// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/origin_gating_cache.h"

#include <algorithm>
#include <variant>

#include "base/containers/map_util.h"
#include "base/notreached.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "url/origin.h"

namespace origin_gating {

OriginGatingCache::OriginGatingCache(CacheScope scope)
    : allowed_navigation_origins_([&] -> StateMap {
        switch (scope) {
          case CacheScope::kOrigin:
            return OriginMap();
          case CacheScope::kSite:
            return SiteMap();
        }
        NOTREACHED();
      }()) {}

OriginGatingCache::~OriginGatingCache() = default;

OriginGatingCache::OriginGatingCache(OriginGatingCache&&) = default;
OriginGatingCache& OriginGatingCache::operator=(OriginGatingCache&&) = default;

bool OriginGatingCache::IsNavigationAllowed(
    const url::Origin& source_origin,
    const url::Origin& destination_origin) const {
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

bool OriginGatingCache::IsNavigationConfirmedByUser(
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

void OriginGatingCache::AllowNavigationTo(url::Origin origin,
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

void OriginGatingCache::AllowNavigationTo(
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

OriginGatingCache::SizeMetrics OriginGatingCache::GetSizeMetrics() const {
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
  return SizeMetrics{static_cast<size_t>(total),
                     static_cast<size_t>(user_confirmed_total)};
}

}  // namespace origin_gating
