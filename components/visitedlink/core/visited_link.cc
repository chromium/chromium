// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visitedlink/core/visited_link.h"

namespace visitedlink {

std::optional<VisitedLink> VisitedLink::MaybeCreateSelfLink() const {
  // We only support self-links for top-level frames and same-origin
  // subframes. If the top-level origin and frame origin are not equal, return
  // std::nullopt. This mitigates the risk of cross-site leaks.
  if (frame_origin != url::Origin::Create(top_level_site.GetURL())) {
    return std::nullopt;
  }
  // Construct the self-link and ensure it is a valid VisitedLink.
  VisitedLink self_link = {link_url, net::SchemefulSite(link_url),
                           url::Origin::Create(link_url)};
  if (self_link.IsValid()) {
    return self_link;
  }
  // If the self-link is invalid, we cannot return its value.
  return std::nullopt;
}

bool VisitedLink::IsValid() const {
  return link_url.is_valid() && !top_level_site.opaque() &&
         !frame_origin.opaque();
}

bool operator==(const VisitedLink& lhs, const VisitedLink& rhs) {
  return std::tie(lhs.link_url, lhs.top_level_site, lhs.frame_origin) ==
         std::tie(rhs.link_url, rhs.top_level_site, rhs.frame_origin);
}

bool operator!=(const VisitedLink& lhs, const VisitedLink& rhs) {
  return !(lhs == rhs);
}

bool operator<(const VisitedLink& lhs, const VisitedLink& rhs) {
  return std::tie(lhs.link_url, lhs.top_level_site, lhs.frame_origin) <
         std::tie(rhs.link_url, rhs.top_level_site, rhs.frame_origin);
}

}  // namespace visitedlink
