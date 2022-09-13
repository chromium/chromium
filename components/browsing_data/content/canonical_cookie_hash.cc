// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/canonical_cookie_hash.h"

#include "base/hash/hash.h"
#include "net/cookies/canonical_cookie.h"

namespace canonical_cookie {

size_t FastHash(const net::CanonicalCookie& cookie) {
  return base::PersistentHash(cookie.Name()) +
         3 * base::PersistentHash(cookie.Domain()) +
         7 * base::PersistentHash(cookie.Path()) +
         (cookie.IsPartitioned()
              ? 13 * base::PersistentHash(
                         cookie.PartitionKey()->site().GetURL().host())
              : 0);
}

bool CanonicalCookieComparer::operator()(
    const net::CanonicalCookie& cookie1,
    const net::CanonicalCookie& cookie2) const {
  return cookie1.Name() == cookie2.Name() &&
         cookie1.Domain() == cookie2.Domain() &&
         cookie1.Path() == cookie2.Path() &&
         cookie1.PartitionKey() == cookie2.PartitionKey();
}

}  // namespace canonical_cookie
