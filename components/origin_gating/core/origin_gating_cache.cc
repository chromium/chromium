// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/origin_gating_cache.h"

#include <utility>
#include <variant>

#include "base/notreached.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "url/origin.h"

namespace origin_gating {

OriginGatingCache::OriginGatingCache(CacheScope scope)
    : allowed_navigation_origins_([&] -> OriginOrSiteSet {
        switch (scope) {
          case CacheScope::kOrigin:
            return OriginSet();
          case CacheScope::kSite:
            return SiteSet();
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
          [&](const OriginSet& origins) -> bool {
            return source_origin == destination_origin ||
                   origins.contains(destination_origin);
          },
          [&](const SiteSet& sites) -> bool {
            return net::SchemefulSite::IsSameSite(source_origin,
                                                  destination_origin) ||
                   sites.contains(net::SchemefulSite(destination_origin));
          },
      },
      allowed_navigation_origins_);
}

bool OriginGatingCache::IsNavigationConfirmedByUser(
    const url::Origin& origin) const {
  return confirmed_navigation_sites_.contains(net::SchemefulSite(origin));
}

void OriginGatingCache::AllowNavigationTo(url::Origin origin,
                                          bool is_user_confirmed) {
  if (is_user_confirmed) {
    confirmed_navigation_sites_.insert(net::SchemefulSite(origin));
  }
  std::visit(absl::Overload{
                 [&](OriginSet& origins) { origins.insert(std::move(origin)); },
                 [&](SiteSet& sites) {
                   sites.insert(net::SchemefulSite(std::move(origin)));
                 },
             },
             allowed_navigation_origins_);
}

void OriginGatingCache::AllowNavigationTo(
    const absl::flat_hash_set<url::Origin>& origins) {
  std::visit(absl::Overload{
                 [&](OriginSet& origin_set) {
                   origin_set.insert(origins.begin(), origins.end());
                 },
                 [&](SiteSet& site_set) {
                   for (const auto& origin : origins) {
                     site_set.insert(net::SchemefulSite(origin));
                   }
                 },
             },
             allowed_navigation_origins_);
}

OriginGatingCache::SizeMetrics OriginGatingCache::GetSizeMetrics() const {
  size_t total = std::visit([](const auto& set) { return set.size(); },
                            allowed_navigation_origins_);
  return SizeMetrics{total, confirmed_navigation_sites_.size()};
}

}  // namespace origin_gating
