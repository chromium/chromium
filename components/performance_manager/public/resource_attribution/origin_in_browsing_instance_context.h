// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_ORIGIN_IN_BROWSING_INSTANCE_CONTEXT_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_ORIGIN_IN_BROWSING_INSTANCE_CONTEXT_H_

#include <compare>
#include <string>
#include <tuple>

#include "content/public/browser/browsing_instance_id.h"
#include "url/origin.h"

namespace resource_attribution {

class OriginInBrowsingInstanceContext {
 public:
  // Creates an OriginInBrowsingInstanceContext covering all frames and workers
  // in a browsing instance with the origin `origin`. Since the set of frames
  // and workers changes over time, the OriginInBrowsingInstanceContext refers
  // to an aggregate of resource usage for a changing set of other contexts.
  OriginInBrowsingInstanceContext(
      const url::Origin& origin,
      content::BrowsingInstanceId browsing_instance);
  ~OriginInBrowsingInstanceContext();

  OriginInBrowsingInstanceContext(const OriginInBrowsingInstanceContext& other);
  OriginInBrowsingInstanceContext& operator=(
      const OriginInBrowsingInstanceContext& other);
  OriginInBrowsingInstanceContext(OriginInBrowsingInstanceContext&& other);
  OriginInBrowsingInstanceContext& operator=(
      OriginInBrowsingInstanceContext&& other);

  // Returns the origin this context covers.
  url::Origin GetOrigin() const;

  // Returns the browsing instance this context covers.
  content::BrowsingInstanceId GetBrowsingInstance() const;

  // Returns a string representation of the context for debugging. This matches
  // the interface of base::TokenType and base::UnguessableToken, for
  // convenience.
  std::string ToString() const;

  // Compare OriginInBrowsingInstanceContexts by origin and browsing instance.
  constexpr friend std::weak_ordering operator<=>(
      const OriginInBrowsingInstanceContext& a,
      const OriginInBrowsingInstanceContext& b) {
    // url::Origin doesn't define operator<=>.
    const auto a_tuple = std::tie(a.origin_, a.browsing_instance_);
    const auto b_tuple = std::tie(b.origin_, b.browsing_instance_);
    if (a_tuple < b_tuple) {
      return std::weak_ordering::less;
    }
    if (a_tuple == b_tuple) {
      return std::weak_ordering::equivalent;
    }
    return std::weak_ordering::greater;
  }

  // Test OriginInBrowsingInstanceContexts for equality by origin and browsing
  // instance.
  constexpr friend bool operator==(const OriginInBrowsingInstanceContext& a,
                                   const OriginInBrowsingInstanceContext& b) {
    return a.origin_ == b.origin_ &&
           a.browsing_instance_ == b.browsing_instance_;
  }

 private:
  url::Origin origin_;
  content::BrowsingInstanceId browsing_instance_;
};

}  // namespace resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_ORIGIN_IN_BROWSING_INSTANCE_CONTEXT_H_
