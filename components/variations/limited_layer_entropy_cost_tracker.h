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
  // used by the layer. This method returns true if there is enough entropy
  // remaining to handle the study assignment or if the study does not consume
  // entropy on the limited layer.
  //
  // Note that this expects the study to be assigned to the same limited layer
  // given in the constructor.
  bool AddEntropyUsedByStudy(const Study& study);

  // Returns the maximum member-level entropy used by studies currently assigned
  // to the limited layer.
  double GetMaxEntropyUsedForTesting() const;

  // Returns true if the total entropy currently used by the limited layer is
  // over the allowed entropy limit.
  bool IsEntropyLimitExceeded() const { return entropy_limit_exceeded_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(LimitedLayerEntropyCostTrackerTest,
                           TestConstructor_WithLimitedLayer);
  FRIEND_TEST_ALL_PREFIXES(LimitedLayerEntropyCostTrackerTest,
                           TestConstructor_LayerMembersUsingEntropyAboveLimit);
  FRIEND_TEST_ALL_PREFIXES(LimitedLayerEntropyCostTrackerTest,
                           TestAddEntropyUsedByStudy_MultipleStudies);
  FRIEND_TEST_ALL_PREFIXES(
      LimitedLayerEntropyCostTrackerTest,
      TestAddEntropyUsedByStudy_LaunchedAndActiveStudies);

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

  // Entropy used by each layer member keyed by its ID.
  absl::flat_hash_map<uint32_t, double> entropy_used_by_member_id_;

  // Whether the entropy limit has been exceeded.
  bool entropy_limit_exceeded_ = false;

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
