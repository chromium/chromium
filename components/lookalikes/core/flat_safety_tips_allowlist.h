// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LOOKALIKES_CORE_FLAT_SAFETY_TIPS_ALLOWLIST_H_
#define COMPONENTS_LOOKALIKES_CORE_FLAT_SAFETY_TIPS_ALLOWLIST_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/function_ref.h"

namespace reputation {
class SafetyTipsConfig;
}  // namespace reputation

namespace lookalikes {

// Compact copy of the URL allowlist of a `reputation::SafetyTipsConfig`: its
// `allowed_pattern`, `canonical_pattern` and `cohort` fields. The component
// data has thousands of these small messages, and keeping them parsed costs
// several times the size of their strings. Here all pattern strings share one
// buffer and all index lists share another, and each record refers to them by
// (offset, length).
//
// Records keep the proto order, so indices keep their meaning, and lookups
// return the same results as `IsUrlAllowlistedBySafetyTipsComponent()` on the
// original proto.
class FlatSafetyTipsAllowlist {
 public:
  // Moves the allowlist of `config` into a new object, which is never null,
  // and frees those fields in `config`. Other fields are left as they are.
  // Offsets are 32-bit, which fits any parsed config since protobuf messages
  // are limited to 2 GiB; larger allowlists crash.
  static std::unique_ptr<FlatSafetyTipsAllowlist> ExtractFrom(
      reputation::SafetyTipsConfig& config);

  FlatSafetyTipsAllowlist(const FlatSafetyTipsAllowlist&) = delete;
  FlatSafetyTipsAllowlist& operator=(const FlatSafetyTipsAllowlist&) = delete;
  ~FlatSafetyTipsAllowlist();

  // Returns whether one of `visited_patterns` is allowed to spoof the
  // canonical URL, whose patterns `get_canonical_url_patterns` returns. It is
  // called at most once, and only if a matching entry is limited to cohorts.
  bool IsUrlAllowlisted(base::span<const std::string> visited_patterns,
                        base::FunctionRef<std::vector<std::string>()>
                            get_canonical_url_patterns) const;

  // Adds the allowlist back to `config`, whose allowlist fields must be empty.
  void CopyToForTesting(reputation::SafetyTipsConfig& config) const;

 private:
  // A range of `strings_` or `indices_`.
  struct Slice {
    uint32_t offset = 0;
    uint32_t length = 0;
  };

  struct AllowedPattern {
    Slice pattern;
    Slice cohort_indices;
  };

  struct Cohort {
    Slice allowed_indices;
    Slice canonical_indices;
  };

  FlatSafetyTipsAllowlist();

  std::string_view GetString(Slice slice) const LIFETIME_BOUND;
  base::span<const uint32_t> GetIndices(Slice slice) const LIFETIME_BOUND;
  bool IsInCohort(const Cohort& cohort,
                  base::span<const std::string> canonical_url_patterns) const;

  // All pattern strings, concatenated.
  std::string strings_;
  // All index lists, concatenated.
  std::vector<uint32_t> indices_;
  // Records for `allowed_pattern`, `canonical_pattern` and `cohort`, in proto
  // order.
  std::vector<AllowedPattern> allowed_patterns_;
  std::vector<Slice> canonical_patterns_;
  std::vector<Cohort> cohorts_;
};

}  // namespace lookalikes

#endif  // COMPONENTS_LOOKALIKES_CORE_FLAT_SAFETY_TIPS_ALLOWLIST_H_
