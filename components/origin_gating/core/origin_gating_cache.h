// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_GATING_CORE_ORIGIN_GATING_CACHE_H_
#define COMPONENTS_ORIGIN_GATING_CORE_ORIGIN_GATING_CACHE_H_

#include <variant>

#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "url/origin.h"

namespace origin_gating {

// This class acts as a cache for origin- or site-keyed decisions. Permission
// to navigate to an origin/site can be recorded, optionally recording that user
// confirmation was obtained for that permission. User confirmations are always
// cached by site.
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

  // Returns true iff navigation to or interaction with the site of `origin` has
  // been allowed (via a previous call to `AllowNavigationTo`) with
  // `is_user_confirmed` set to true.
  bool IsNavigationConfirmedByUser(const url::Origin& origin) const;

  // Adds the given origin to the set of origins to which the actor is allowed
  // to navigate. If `is_user_confirmed` is true, the site of `origin` is marked
  // as confirmed by the user, so `IsNavigationConfirmedByUser` will return true
  // for any origin on this site in the future.
  void AllowNavigationTo(url::Origin origin, bool is_user_confirmed);

  // Adds the given origins to the set of origins to which the actor is allowed
  // to navigate. The origins are considered non-user-confirmed.
  void AllowNavigationTo(const absl::flat_hash_set<url::Origin>& origins);

  // Returns size metrics for UMA logging.
  SizeMetrics GetSizeMetrics() const;

 private:
  using OriginSet = absl::flat_hash_set<url::Origin>;
  using SiteSet = absl::flat_hash_set<net::SchemefulSite>;
  using OriginOrSiteSet = std::variant<OriginSet, SiteSet>;

  // The set of origins/sites which the browser is allowed to navigate to under
  // actor control. Note that presence in this set does *not* imply that the
  // actor may navigate without confirming with the user first. This set can
  // have origins/sites added to it by the server actions or by confirming the
  // new origin with the model or user. Sensitive origins/sites that are on the
  // optimization guide blocklist are not exempt by this set.
  OriginOrSiteSet allowed_navigation_origins_;

  // The set of sites for which user confirmation has been obtained.
  SiteSet confirmed_navigation_sites_;
};

}  // namespace origin_gating

#endif  // COMPONENTS_ORIGIN_GATING_CORE_ORIGIN_GATING_CACHE_H_
