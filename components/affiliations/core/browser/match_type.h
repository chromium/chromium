// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AFFILIATIONS_CORE_BROWSER_MATCH_TYPE_H_
#define COMPONENTS_AFFILIATIONS_CORE_BROWSER_MATCH_TYPE_H_

#include <optional>

namespace affiliations {

// Enum describing how a credential/domain was matched for a given origin. This
// enum is a bitmask because a credential/domain can be matched by multiple
// sources.
enum class MatchType {
  // Default match type meaning it is identical to a requested URL.
  kExact = 0,
  // It is affiliated with a given URL.
  // Affiliation information is provided by the affiliation service.
  kAffiliated = 1 << 1,
  // It has the same eTLD+1 as a given URL.
  kPSL = 1 << 2,
  // It is grouped with a given URL. Grouping
  // information is provided by the affiliation service.
  kGrouped = 1 << 3,
};

constexpr MatchType operator&(MatchType lhs, MatchType rhs) {
  return static_cast<MatchType>(static_cast<int>(lhs) & static_cast<int>(rhs));
}

constexpr MatchType operator|(MatchType lhs, MatchType rhs) {
  return static_cast<MatchType>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

constexpr void operator|=(std::optional<MatchType>& lhs, MatchType rhs) {
  lhs = lhs.has_value() ? (lhs.value() | rhs) : rhs;
}

}  // namespace affiliations

#endif  // COMPONENTS_AFFILIATIONS_CORE_BROWSER_MATCH_TYPE_H_
