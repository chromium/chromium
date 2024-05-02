// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visitedlink/core/visited_link.h"

namespace visitedlink {

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
