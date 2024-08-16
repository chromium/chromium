// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITEDLINK_CORE_VISITED_LINK_H_
#define COMPONENTS_VISITEDLINK_CORE_VISITED_LINK_H_

#include "net/base/schemeful_site.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace visitedlink {
// A VisitedLink contains the triple-partition key for a given :visited link.
// The triple-partition key consists of a <link url, top-level site, frame
// origin> where:
//   - `link url` is the link we have visited.
//   - `top_level_site` is the top-level frame where the link url was visited
//   from represented as a schemeful site.
//   - `frame_origin` is frame where the link url was visited from represented
//   as an origin.
struct VisitedLink {
  // Returns the "self-link" version of a VisitedLink, i.e. a link with the
  // following triple key: <link_url, link_url, link_url>. If the resulting
  // triple-key is not valid, will return std::nullopt. We only support
  // self-links for top-level frames and same-origin subframes. If the top-level
  // origin and frame origin are not equal, we return std::nullopt.
  std::optional<VisitedLink> MaybeCreateSelfLink() const;
  // A VisitedLink is valid if its components are valid and not opaque.
  bool IsValid() const;

  GURL link_url;
  net::SchemefulSite top_level_site;
  url::Origin frame_origin;

 private:
  friend bool operator==(const VisitedLink& lhs, const VisitedLink& rhs);
  friend bool operator!=(const VisitedLink& lhs, const VisitedLink& rhs);
  friend bool operator<(const VisitedLink& lhs, const VisitedLink& rhs);
};

}  // namespace visitedlink

#endif  // COMPONENTS_VISITEDLINK_CORE_VISITED_LINK_H_
