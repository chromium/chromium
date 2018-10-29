// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/first_party_origin.h"

#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace subresource_filter {

namespace {

bool IsThirdPartyImpl(const GURL& url, const url::Origin& first_party_origin) {
  return !net::registry_controlled_domains::SameDomainOrHost(
      url, first_party_origin,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

}  // namespace

FirstPartyOrigin::FirstPartyOrigin(url::Origin document_origin)
    : document_origin_(std::move(document_origin)) {}

bool FirstPartyOrigin::IsThirdParty(const GURL& url) const {
  if (document_origin_.opaque())
    return true;
  base::StringPiece host_piece = url.host_piece();
  if (!last_checked_host_.empty() && host_piece == last_checked_host_)
    return last_checked_host_was_third_party_;

  last_checked_host_.assign(host_piece.data(), host_piece.size());
  last_checked_host_was_third_party_ = IsThirdPartyImpl(url, document_origin_);
  return last_checked_host_was_third_party_;
}

bool FirstPartyOrigin::IsThirdParty(const GURL& url,
                                    const url::Origin& first_party_origin) {
  return first_party_origin.opaque() ||
         IsThirdPartyImpl(url, first_party_origin);
}

}  // namespace subresouce_filter
