// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_GATING_CORE_ORIGIN_GATING_CACHE_H_
#define COMPONENTS_ORIGIN_GATING_CORE_ORIGIN_GATING_CACHE_H_

#include <variant>

#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "url/origin.h"

namespace origin_gating {

// This class acts as a cache for origin- or site-keyed decisions. Permission
// to navigate to an origin/site can be recorded, optionally recording that user
// confirmation was obtained for that permission.
class OriginGatingCache {
 public:
  struct SizeMetrics {
    friend bool operator==(const SizeMetrics&, const SizeMetrics&) = default;

    size_t allow_list_size = 0;
    size_t confirmed_list_size = 0;
  };

  // The level of granularity at which to cache gating decisions.
  enum class CacheScope { kOrigin, kSite };

  explicit OriginGatingCache(CacheScope scope);
  ~OriginGatingCache();

  OriginGatingCache(const OriginGatingCache&) = delete;
  OriginGatingCache& operator=(const OriginGatingCache&) = delete;
  OriginGatingCache(OriginGatingCache&&);
  OriginGatingCache& operator=(OriginGatingCache&&);

  // Returns true iff navigation to `destination_origin` is allowed, either
  // because the source and destination are considered the "same", or by a
  // previous call to `AllowNavigation`.
  bool IsNavigationAllowed(const url::Origin& source_origin,
                           const url::Origin& destination_origin) const;

  // Returns true iff navigation to or interaction with `origin` has been
  // allowed (via a previous call to `AllowNavigationTo`) with
  // `is_user_confirmed` set to true.
  bool IsNavigationConfirmedByUser(const url::Origin& origin) const;

  // Adds the given origin to the set of origins to which the actor is allowed
  // to navigate. `is_user_confirmed` controls whether
  // `IsNavigationConfirmedByUser` will return true for this origin in the
  // future.
  void AllowNavigationTo(url::Origin origin, bool is_user_confirmed);

  // Adds the given origins to the set of origins to which the actor is allowed
  // to navigate. The origins are considered non-user-confirmed.
  void AllowNavigationTo(const absl::flat_hash_set<url::Origin>& origins);

  // Returns size metrics for UMA logging.
  SizeMetrics GetSizeMetrics() const;

 private:
  struct State {
    // Whether the user has explicitly confirmed navigation to this origin/site.
    bool is_user_confirmed = false;
  };

  using OriginMap = absl::flat_hash_map<url::Origin, State>;
  using SiteMap = absl::flat_hash_map<net::SchemefulSite, State>;
  using StateMap = std::variant<OriginMap, SiteMap>;

  // The set of origins/sites which the browser is allowed to navigate to under
  // actor control. Note that presence in this map does *not* imply that the
  // actor may navigate without confirming with the user first. This set can
  // have origins/sites added to it by the server actions or by confirming the
  // new origin with the model or user. Sensitive origins/sites that are on the
  // optimization guide blocklist are not exempt by this set.
  StateMap allowed_navigation_origins_;
};

}  // namespace origin_gating

#endif  // COMPONENTS_ORIGIN_GATING_CORE_ORIGIN_GATING_CACHE_H_
