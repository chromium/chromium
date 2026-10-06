// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lookalikes/core/flat_safety_tips_allowlist.h"

#include <algorithm>
#include <functional>
#include <optional>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "components/lookalikes/core/safety_tips.pb.h"

namespace lookalikes {

namespace {

// Frees the elements of `field`. `Clear()` would keep them allocated.
template <typename T>
void ClearAndFree(google::protobuf::RepeatedPtrField<T>& field) {
  google::protobuf::RepeatedPtrField<T>().Swap(&field);
}

}  // namespace

FlatSafetyTipsAllowlist::FlatSafetyTipsAllowlist() = default;

FlatSafetyTipsAllowlist::~FlatSafetyTipsAllowlist() = default;

// static
std::unique_ptr<FlatSafetyTipsAllowlist> FlatSafetyTipsAllowlist::ExtractFrom(
    reputation::SafetyTipsConfig& config) {
  // These sums can't overflow, since they measure data already in memory.
  size_t string_size = 0;
  size_t index_count = 0;
  for (const auto& allowed : config.allowed_pattern()) {
    string_size += allowed.pattern().size();
    index_count += static_cast<size_t>(allowed.cohort_index_size());
  }
  for (const auto& canonical : config.canonical_pattern()) {
    string_size += canonical.pattern().size();
  }
  for (const auto& cohort : config.cohort()) {
    index_count += static_cast<size_t>(cohort.allowed_index_size()) +
                   static_cast<size_t>(cohort.canonical_index_size());
  }

  auto allowlist = base::WrapUnique(new FlatSafetyTipsAllowlist());
  allowlist->strings_.reserve(string_size);
  allowlist->indices_.reserve(index_count);
  allowlist->allowed_patterns_.reserve(
      static_cast<size_t>(config.allowed_pattern_size()));
  allowlist->canonical_patterns_.reserve(
      static_cast<size_t>(config.canonical_pattern_size()));
  allowlist->cohorts_.reserve(static_cast<size_t>(config.cohort_size()));

  auto add_string = [&allowlist](const std::string& value) {
    Slice slice{base::checked_cast<uint32_t>(allowlist->strings_.size()),
                base::checked_cast<uint32_t>(value.size())};
    allowlist->strings_.append(value);
    return slice;
  };
  auto add_indices =
      [&allowlist](const google::protobuf::RepeatedField<uint32_t>& values) {
        Slice slice{base::checked_cast<uint32_t>(allowlist->indices_.size()),
                    base::checked_cast<uint32_t>(values.size())};
        allowlist->indices_.insert(allowlist->indices_.end(), values.begin(),
                                   values.end());
        return slice;
      };

  for (const auto& allowed : config.allowed_pattern()) {
    allowlist->allowed_patterns_.push_back(
        {add_string(allowed.pattern()), add_indices(allowed.cohort_index())});
  }
  for (const auto& canonical : config.canonical_pattern()) {
    allowlist->canonical_patterns_.push_back(add_string(canonical.pattern()));
  }
  for (const auto& cohort : config.cohort()) {
    allowlist->cohorts_.push_back({add_indices(cohort.allowed_index()),
                                   add_indices(cohort.canonical_index())});
  }
  // The proto requires `allowed_pattern` to be sorted, since lookups
  // binary-search it.
  DCHECK(std::ranges::is_sorted(allowlist->allowed_patterns_,
                                std::ranges::less(),
                                [&allowlist](const AllowedPattern& entry) {
                                  return allowlist->GetString(entry.pattern);
                                }));

  ClearAndFree(*config.mutable_allowed_pattern());
  ClearAndFree(*config.mutable_canonical_pattern());
  ClearAndFree(*config.mutable_cohort());
  return allowlist;
}

bool FlatSafetyTipsAllowlist::IsUrlAllowlisted(
    base::span<const std::string> visited_patterns,
    base::FunctionRef<std::vector<std::string>()> get_canonical_url_patterns)
    const {
  std::optional<std::vector<std::string>> canonical_url_patterns;
  for (const std::string& pattern : visited_patterns) {
    // Like the proto lookup, only the first entry equal to `pattern` counts.
    auto it = std::ranges::lower_bound(
        allowed_patterns_, std::string_view(pattern), std::ranges::less(),
        [this](const AllowedPattern& entry) {
          return GetString(entry.pattern);
        });
    if (it == allowed_patterns_.end() || GetString(it->pattern) != pattern) {
      continue;
    }

    base::span<const uint32_t> cohort_indices = GetIndices(it->cohort_indices);
    // An entry without cohorts may spoof any site.
    if (cohort_indices.empty()) {
      return true;
    }
    for (const uint32_t cohort_index : cohort_indices) {
      // Invalid indices are ignored.
      if (cohort_index >= cohorts_.size()) {
        continue;
      }
      if (!canonical_url_patterns) {
        canonical_url_patterns = get_canonical_url_patterns();
      }
      if (IsInCohort(cohorts_[cohort_index], *canonical_url_patterns)) {
        return true;
      }
    }
  }
  return false;
}

void FlatSafetyTipsAllowlist::CopyToForTesting(
    reputation::SafetyTipsConfig& config) const {
  CHECK(config.allowed_pattern().empty());
  CHECK(config.canonical_pattern().empty());
  CHECK(config.cohort().empty());
  for (const AllowedPattern& entry : allowed_patterns_) {
    reputation::UrlPattern* allowed = config.add_allowed_pattern();
    allowed->set_pattern(std::string(GetString(entry.pattern)));
    for (const uint32_t index : GetIndices(entry.cohort_indices)) {
      allowed->add_cohort_index(index);
    }
  }
  for (const Slice canonical : canonical_patterns_) {
    config.add_canonical_pattern()->set_pattern(
        std::string(GetString(canonical)));
  }
  for (const Cohort& entry : cohorts_) {
    reputation::Cohort* cohort = config.add_cohort();
    for (const uint32_t index : GetIndices(entry.allowed_indices)) {
      cohort->add_allowed_index(index);
    }
    for (const uint32_t index : GetIndices(entry.canonical_indices)) {
      cohort->add_canonical_index(index);
    }
  }
}

std::string_view FlatSafetyTipsAllowlist::GetString(Slice slice) const {
  return std::string_view(strings_).substr(slice.offset, slice.length);
}

base::span<const uint32_t> FlatSafetyTipsAllowlist::GetIndices(
    Slice slice) const {
  return base::span(indices_).subspan(slice.offset, slice.length);
}

bool FlatSafetyTipsAllowlist::IsInCohort(
    const Cohort& cohort,
    base::span<const std::string> canonical_url_patterns) const {
  const base::span<const uint32_t> allowed_indices =
      GetIndices(cohort.allowed_indices);
  const base::span<const uint32_t> canonical_indices =
      GetIndices(cohort.canonical_indices);
  // Allowed and canonical members are both valid spoof targets. Invalid
  // indices are ignored.
  for (const std::string& pattern : canonical_url_patterns) {
    for (const uint32_t index : allowed_indices) {
      if (index < allowed_patterns_.size() &&
          GetString(allowed_patterns_[index].pattern) == pattern) {
        return true;
      }
    }
    for (const uint32_t index : canonical_indices) {
      if (index < canonical_patterns_.size() &&
          GetString(canonical_patterns_[index]) == pattern) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace lookalikes
