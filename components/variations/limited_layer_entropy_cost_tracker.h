// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_LIMITED_LAYER_ENTROPY_COST_TRACKER_H_
#define COMPONENTS_VARIATIONS_LIMITED_LAYER_ENTROPY_COST_TRACKER_H_

#include <cstdint>

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace variations {

class Layer;
class Study;

// Provides methods to calculate and monitor the total entropy used by studies
// assigned to a limited layer.
class COMPONENT_EXPORT(VARIATIONS) LimitedLayerEntropyCostTracker {
 public:
  // Tracks the entropy consumed by studies that reference the given layer
  // (i.e. studies the caller passes to AddEntropyUsedByStudy()).
  //
  //   * layer - The layer whose entropy is being tracked.
  //   * entropy_limit_in_bits - The maximum allowed entropy limit for any
  //   member of the limited layer.
  //
  // The tracker expects that the layer and study data passed to its constructor
  // and methods are valid. However, as this data comes from external sources
  // (i.e. the seed), the tracker performs additional validation on its input
  // and will DumpWithoutCrashing() to log the stack trace by which the invalid
  // input was provided. If this occurs, the tracker will be invalidated and
  // the seed from which the tracker is derived should be rejected.
  LimitedLayerEntropyCostTracker(const Layer& layer,
                                 double entropy_limit_in_bits);
  ~LimitedLayerEntropyCostTracker();

  LimitedLayerEntropyCostTracker(const LimitedLayerEntropyCostTracker&) =
      delete;
  LimitedLayerEntropyCostTracker& operator=(
      const LimitedLayerEntropyCostTracker&) = delete;

  // Returns true if the tracker is valid.
  bool IsValid() const { return is_valid_; }

  // Calculates the entropy used by the study and adds it to the total entropy
  // tracked for each referenced layer member. A return value of false means
  // that the tracker has been invalidated by invalid input. A return value of
  // true means that the study's entropy has been successfully added to the
  // tracker. It does NOT mean that the entropy limit has not been exceeded.
  // Callers should check `IsEntropyLimitExceeded()` to determine if the entropy
  // limit has been exceeded.
  //
  // Note that this expects the study to be assigned to the same limited layer
  // given in the constructor.
  bool AddEntropyUsedByStudy(const Study& study);

  // Returns true if the total entropy currently used by the limited layer is
  // over the allowed entropy limit.
  bool IsEntropyLimitExceeded() const;

  // Exposed for testing only.
  double GetMaxEntropyUsedForTesting() const { return GetMaxEntropyUsed(); }

 private:
  FRIEND_TEST_ALL_PREFIXES(LimitedLayerEntropyCostTrackerTest,
                           TestConstructor_WithLimitedLayer);
  FRIEND_TEST_ALL_PREFIXES(LimitedLayerEntropyCostTrackerTest,
                           TestConstructor_LayerMembersUsingEntropyAboveLimit);
  FRIEND_TEST_ALL_PREFIXES(LimitedLayerEntropyCostTrackerTest,
                           TestAddEntropyUsedByStudy_MultipleStudies);
  FRIEND_TEST_ALL_PREFIXES(LimitedLayerEntropyCostTrackerTest,
                           TestAddEntropyUsedByStudy_LaunchedAndActiveStudies);
  FRIEND_TEST_ALL_PREFIXES(LimitedLayerEntropyCostTrackerTest,
                           TestAddEntropyUsedByStudy_IsTimeAware);

  // Returns the maximum member-level entropy used by studies currently being
  // tracked by the tracker. This method is idempotent and logically const, but
  // it does modify the internal state of the tracker by sorting the entropy
  // events in place.
  double GetMaxEntropyUsed() const;

  // Invalidates the tracker on bad input. Note that this is a terminal state
  // for the tracker. Once the tracker is invalidated, it cannot be made valid
  // again.
  void Invalidate();

  // The maximum allowed entropy limit for any member of the limited layer.
  const double entropy_limit_in_bits_;

  // ID of the active limited layer for this client. This is used to sanity
  // check the studies whose entropy is being tracked (they should all refer
  // to the same limited layer ID).
  const uint32_t limited_layer_id_;

  // Each entropy event is a (timestamp, entropy) change pair, denoting that
  // entropy is added/removed at a certain time, as studies start or end their
  // visibility. `timestamp` is the time in seconds since Unix epoch, UTC.
  // `entropy` is the positive or negative entropy change. We take advantage of
  // the default ordering of std::pair to order the events by time then by
  // entropy change. The sort order means that we remove entropy before adding
  // entropy when events have the same timestamp.
  using EntropyEvent = std::pair<int64_t, double>;
  using EntropyEventList = std::vector<EntropyEvent>;

  // Entropy events by each layer member keyed by its ID. This is mutable in
  // order to compute the entropy events on demand in `GetMaxEntropyUsed()`,
  // which performs an in-place sort on the entropy events.
  mutable absl::flat_hash_map<uint32_t, EntropyEventList>
      entropy_events_by_member_id_;

  // Whether the tracker has had non-zero entropy added for at least one study.
  // i.e., the entropy is not solely based on the layer member sizes.
  bool includes_study_entropy_ = false;

  // Whether all input given to the tracker has been valid. If the tracker is
  // invalidated by bad input, the seed from which the input is derived should
  // be rejected.
  bool is_valid_ = true;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_LIMITED_LAYER_ENTROPY_COST_TRACKER_H_
