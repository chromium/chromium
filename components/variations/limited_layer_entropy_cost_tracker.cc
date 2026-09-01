// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/limited_layer_entropy_cost_tracker.h"

#include <math.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <ranges>

#include "base/check_op.h"
#include "base/debug/dump_without_crashing.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "components/variations/experiment_group_ids.h"
#include "components/variations/variations_layers.h"

namespace variations {
namespace {

constexpr uint32_t kInvalidLayerId = 0u;
constexpr uint32_t kInvalidLayerMemberId = 0u;
constexpr int64_t kMinTimeStamp = 0;
constexpr int64_t kMaxTimeStamp = std::numeric_limits<int64_t>::max();

// Returns the given timestamp if it is positive, otherwise returns the given
// default value.
int64_t GetTimestamp(int64_t t, int64_t default_if_not_valid) {
  return t <= kMinTimeStamp ? default_if_not_valid : t;
}

int64_t GetWebVisibilityStartTime(const Study& study) {
  return std::max(
      GetTimestamp(study.filter().start_date(), kMinTimeStamp),
      GetTimestamp(study.google_web_visibility_start_date(), kMinTimeStamp));
}

int64_t GetWebVisibilityEndTime(const Study& study) {
  return std::min(
      GetTimestamp(study.filter().end_date(), kMaxTimeStamp),
      GetTimestamp(study.google_web_visibility_end_date(), kMaxTimeStamp));
}

// Converts a probability value (represented by numerator/denominator) to an
// entropy value. Callers should ensure that both arguments are strictly
// positive and that `numerator` <= `denominator`. This always returns a
// non-negative number.
double ConvertToBitsOfEntropy(uint64_t numerator, uint64_t denominator) {
  CHECK_GT(numerator, 0u);
  CHECK_LE(numerator, denominator);
  return -log2(base::strict_cast<double>(numerator) /
               base::strict_cast<double>(denominator));
}

// Returns the amount of entropy (in bits) used by `study`. Expected to be
// called only for studies for which `ConsumesEntropy()` returns true, and
// CHECKs this.
double GetEntropyUsedByStudy(const Study& study) {
  CHECK(ConsumesEntropy(study));

  // Use uint32_t to match the type of `probability_weight` field in the
  // experiment proto.
  uint32_t min_weight = std::numeric_limits<uint32_t>::max();
  uint64_t total_weight = 0;

  for (const auto& experiment : study.experiment()) {
    // This will CHECK if `total_weight` (a uint64_t) overflows, which is nearly
    // impossible since each `experiment.probability_weight()` is a uint32_t.
    // This is not expected to come up for valid variations seeds in production.
    total_weight = base::CheckAdd(total_weight, experiment.probability_weight())
                       .ValueOrDie();

    // Only consider groups that can contribute to a study's entropy cost.
    if (IsWeightedGroupWithExperimentId(experiment)) {
      min_weight = std::min(min_weight, experiment.probability_weight());
    }
  }

  // `ConvertToBitsOfEntropy()` requires 0 < min_weight <= total_weight, which
  // is implied by `ConsumesEntropy()` returning true, so do a check here.
  CHECK(total_weight > 0);
  CHECK(min_weight <= total_weight);

  // Mathematically, this returns -log2(`min_weight` / `total_weight`).
  // If the probability of a client being assigned to a specific group in the
  // study is p, the entropy revealed by this assignment is -log2(p):
  // https://en.wikipedia.org/wiki/Entropy_(information_theory). Hence, the
  // entropy is maximal for clients assigned to the smallest group in the study.
  return ConvertToBitsOfEntropy(min_weight, total_weight);
}

// Computes the entropy used by the limited layer member.
double GetLayerMemberEntropy(const Layer::LayerMember& member,
                             uint64_t num_slots) {
  uint32_t num_slots_in_member = 0;
  for (const Layer::LayerMember::SlotRange& range : member.slots()) {
    // Adding one since the range is inclusive.
    num_slots_in_member += range.end() - range.start() + 1;
  }
  return ConvertToBitsOfEntropy(num_slots_in_member, num_slots);
}

}  // namespace

LimitedLayerEntropyCostTracker::LimitedLayerEntropyCostTracker(
    const Layer& layer,
    double entropy_limit_in_bits)
    : entropy_limit_in_bits_(entropy_limit_in_bits),
      limited_layer_id_(layer.id()) {
  // The caller should have already validated the layer. However, as the layer
  // data comes from an external source, we verify it here again for safety,
  // instead of using a CHECK. Note that verify each condition individually in
  // order to dump a unique stack trace for each failure condition.
  if (limited_layer_id_ == kInvalidLayerId) {
    Invalidate();
    return;
  }
  if (entropy_limit_in_bits_ <= 0.0) {
    Invalidate();
    return;
  }
  const auto num_slots = layer.num_slots();
  if (num_slots <= 0u) {
    Invalidate();
    return;
  }
  const auto& layer_members = layer.members();
  if (layer_members.empty()) {
    Invalidate();
    return;
  }
  if (layer.entropy_mode() != Layer::LIMITED) {
    Invalidate();
    return;
  }
  if (!VariationsLayers::AreSlotBoundsValid(layer)) {
    Invalidate();
    return;
  }

  // Compute the entropy used by each layer member keyed by its memberID.
  entropy_events_by_member_id_.reserve(layer_members.size());
  for (const auto& member : layer_members) {
    if (member.id() == kInvalidLayerMemberId) {
      Invalidate();
      return;
    }
    auto [iterator, inserted] =
        entropy_events_by_member_id_.emplace(member.id(), EntropyEventList());
    if (!inserted) {
      // => Duplicated layer member ID.
      Invalidate();
      return;
    }

    // Add an entropy event at kMinTimeStamp, representing the initial entropy
    // state of the layer member before any study assignments. I.e., the entropy
    // cost of the layer member itself. Note that this base cost is only counted
    // towards the total entropy used when there is at least one
    // entropy-consuming study referencing this member.
    //
    // Note that we reserve an initial capacity for the entropy events to avoid
    // up to 6-8 (re)allocations per layer member when growing the entropy event
    // vector. This pre-allocation is not expected to exactly match the number
    // of entropy events; rather, it simply avoids the allocation overhead for
    // the growth of the entropy events vector up to the given capacity.
    //
    // Also note that these allocations are lifetime bound to the tracker, which
    // exists transiently during seed validation, so there should be no net
    // impact on memory usage.
    constexpr size_t kInitialCapacity = 64;
    auto& entropy_events = iterator->second;
    entropy_events.reserve(kInitialCapacity);
    entropy_events.emplace_back(kMinTimeStamp,
                                GetLayerMemberEntropy(member, num_slots));
  }
}

LimitedLayerEntropyCostTracker::~LimitedLayerEntropyCostTracker() = default;

bool LimitedLayerEntropyCostTracker::AddEntropyUsedByStudy(const Study& study) {
  if (!IsValid()) {
    return false;
  }
  // The caller should have already validated the study's layer references.
  // However, as the study data comes from an external source, we verify it
  // here again for safety, instead of using a CHECK. Note that verify each
  // condition individually in order to dump a unique stack trace for each
  // failure condition.
  if (!study.has_layer()) {
    Invalidate();
    return false;
  }
  const auto& layer_ref = study.layer();
  if (layer_ref.layer_id() != limited_layer_id_) {
    Invalidate();
    return false;
  }
  const auto& layer_member_ids =
      layer_ref.layer_member_ids().empty()
          ? VariationsLayers::FallbackLayerMemberIds(layer_ref)
          : layer_ref.layer_member_ids();
  if (layer_member_ids.empty()) {
    Invalidate();
    return false;
  }

  for (const uint32_t member_id : layer_member_ids) {
    if (member_id == kInvalidLayerMemberId) {
      Invalidate();
      return false;
    }
    const auto it = entropy_events_by_member_id_.find(member_id);
    if (it == entropy_events_by_member_id_.end()) {
      Invalidate();
      return false;
    }
  }

  if (!ConsumesEntropy(study)) {
    return true;
  }
  members_with_entropy_consuming_studies_.insert(layer_member_ids.begin(),
                                                 layer_member_ids.end());
  const double study_entropy = GetEntropyUsedByStudy(study);
  if (study_entropy <= 0) {
    // In rare cases, a study that consumes entropy may not consume any at the
    // study-level. An example of such a study is one with a single weighted
    // group that happens to set an experiment ID. In this case, entropy
    // consumption comes from the base member cost of the limited-layer members
    // in which the study runs.
    return true;
  }

  const int64_t start_time = GetWebVisibilityStartTime(study);
  const int64_t end_time = GetWebVisibilityEndTime(study);

  // Update the entropy events for each layer member referenced by the study.
  // It is assumed that layer member references have already been validated by
  // the caller.
  for (const uint32_t member_id : layer_member_ids) {
    // Add an entropy event for the study's start and end times. Note that
    // events which free entropy at the end-of-time can (as an optimization)
    // be skipped, as they will not contribute the maximum entropy used.
    auto& entropy_events = entropy_events_by_member_id_[member_id];
    entropy_events.emplace_back(start_time, study_entropy);
    if (end_time != kMaxTimeStamp) {
      entropy_events.emplace_back(end_time, -study_entropy);
    }
  }

  // Returning true means that the studies entropy has been successfully added
  // to the tracker. It does not mean that the entropy limit has not been
  // exceeded. Callers should check `IsEntropyLimitExceeded()` to determine if
  // the entropy limit has been exceeded.
  return true;
}

bool LimitedLayerEntropyCostTracker::IsEntropyLimitExceeded() const {
  return GetMaxEntropyUsed() > entropy_limit_in_bits_;
}

double LimitedLayerEntropyCostTracker::GetMaxEntropyUsed() const {
  double max_entropy_used = 0.0;
  for (const uint32_t member_id : members_with_entropy_consuming_studies_) {
    const auto it = entropy_events_by_member_id_.find(member_id);
    if (it == entropy_events_by_member_id_.end()) {
      continue;
    }
    auto& entropy_events = it->second;
    // Sort the entropy events by increasing time and then by increasing entropy
    // amount. This ensures that we remove entropy (negative amounts) before
    // adding entropy (positive amounts) for identical timestamps.
    std::ranges::sort(entropy_events);
    double entropy_used = 0.0;
    for (const auto& [unused_timestamp, entropy_change] : entropy_events) {
      entropy_used += entropy_change;
      max_entropy_used = std::max(max_entropy_used, entropy_used);
    }
  }
  return max_entropy_used;
}

void LimitedLayerEntropyCostTracker::Invalidate() {
  // The caller should have already validated the layer and study info before
  // any and all calls to the tracker. However, as the layer and study data
  // comes from an external source, there are additional safety checks made
  // throughout the tracker. We use these instead of CHECKS or DCHECKS and
  // verify each condition individually in order to dump a unique stack trace
  // for each failure condition.
  is_valid_ = false;
  base::debug::DumpWithoutCrashing();
}

}  // namespace variations
