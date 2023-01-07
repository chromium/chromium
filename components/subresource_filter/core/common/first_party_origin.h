// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_FIRST_PARTY_ORIGIN_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_FIRST_PARTY_ORIGIN_H_

#include <string>

#include "url/gurl.h"
#include "url/origin.h"

namespace subresource_filter {

// Encapsulates the first-party origin of a document, and provides fast (cached)
// third-partiness checks against that origin, i.e. whether a given URL is
// third-party in relation to the document's origin.
//
// It uses a simple one-entry cache to optimize for the case when a significant
// number of consecutive URLs have the same domain.
class FirstPartyOrigin {
 public:
  explicit FirstPartyOrigin(url::Origin document_origin);

  const url::Origin& origin() const { return document_origin_; }

  // Returns whether |url| is a third-party in relation to |this| origin. May
  // return a cached value. May update the cache.
  bool IsThirdParty(const GURL& url) const;

  // Returns whether |url| is a third party in respect to |first_party_origin|.
  static bool IsThirdParty(const GURL& url,
                           const url::Origin& first_party_origin);

 private:
  const url::Origin document_origin_;

  // One-entry cache that stores input/output of the last IsThirdParty
  // call. To improve performance, the cache stores only the host part of the
  // last checked URL. Checking only the host part before returning the cached
  // result is correct, because registry_controlled_domains::SameDomainOrHost
  // would return the same result for any GURL that has the same non-empty
  // host part as the last checked URL.
  mutable std::string last_checked_host_;
  mutable bool last_checked_host_was_third_party_ = false;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_FIRST_PARTY_ORIGIN_H_
