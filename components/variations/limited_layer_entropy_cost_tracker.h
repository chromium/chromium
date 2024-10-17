// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_LIMITED_LAYER_ENTROPY_COST_TRACKER_H_
#define COMPONENTS_VARIATIONS_LIMITED_LAYER_ENTROPY_COST_TRACKER_H_

#include <map>
#include <set>

#include "base/component_export.h"
#include "base/gtest_prod_util.h"

namespace variations {

class Study;
class VariationsLayers;
class VariationsSeed;

// Provides methods to calculate and monitor the total entropy used by studies
// assigned to a limited layer.
class COMPONENT_EXPORT(VARIATIONS) LimitedLayerEntropyCostTracker {
 public:
  LimitedLayerEntropyCostTracker(const VariationsLayers& layers,
                                 const VariationsSeed& seed,
                                 double entropy_limit_in_bits);
  ~LimitedLayerEntropyCostTracker();

  LimitedLayerEntropyCostTracker(const LimitedLayerEntropyCostTracker&) =
      delete;
  LimitedLayerEntropyCostTracker& operator=(
      const LimitedLayerEntropyCostTracker&) = delete;

  // Calculates the entropy used by the study and adds it to the total entropy
  // used by the layer. This method returns true if there is enough entropy
  // remaining to handle the study assignment or if the study does not consume
  // entropy on the limited layer.
  bool TryAddEntropyUsedByStudy(const Study& study);

  // Returns the total entropy used by studies currently assigned to the limited
  // layer.
  double GetTotalEntropyUsedForTesting();

  // Returns true if the total entropy currently used by the limited layer is
  // over the allowed entropy limit.
  bool IsEntropyLimitReached() const { return entropy_limit_reached_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(LimitedLayerEntropyCostTrackerTest,
                           TestConstructor_WithNoLimitedLayer);
  FRIEND_TEST_ALL_PREFIXES(LimitedLayerEntropyCostTrackerTest,
                           TestConstructor_WithNonLimitedEntropyMode);
  FRIEND_TEST_ALL_PREFIXES(LimitedLayerEntropyCostTrackerTest,
                           TestConstructor_WithNoLayerMembers);
  FRIEND_TEST_ALL_PREFIXES(LimitedLayerEntropyCostTrackerTest,
                           TestConstructor_WithManyLimitedLayers);
  FRIEND_TEST_ALL_PREFIXES(LimitedLayerEntropyCostTrackerTest,
                           TestConstructor_WithMisconfiguredLimitedLayer);
  FRIEND_TEST_ALL_PREFIXES(LimitedLayerEntropyCostTrackerTest,
                           TestConstructor_WithLimitedLayer);
  FRIEND_TEST_ALL_PREFIXES(LimitedLayerEntropyCostTrackerTest,
                           TestConstructor_LayerMembersUsingEntropyAboveLimit);

  static constexpr uint64_t kInvalidLayerId = 0;

  // Id of the active limited layer for this client. A layer is "active" for a
  // client if the client's slot for that layer is associated with a layer
  // member.
  uint32_t active_limited_layer_id_ = kInvalidLayerId;
  const double entropy_limit_in_bits_;
  bool includes_entropy_used_by_studies_ = false;
  bool entropy_limit_reached_ = false;

  // Entropy used by each layer member keyed by its ID. Using uint32_t as the
  // key type since the ID of a layer member proto is a uint32_t.
  std::map<uint32_t, double> entropy_used_by_layer_members_;

  // Contains the Ids of all limited layers in the seed. This field is used in
  // `TryAddEntropyUsedByStudy` to check if a study references a limited layer
  // because the Study class does not contain enough information to perform that
  // check.
  // TODO(siakabaro): If a study references a layer, cache the layer information
  // in the ProcessedStudy class and update `TryAddEntropyUsedByStudy` to take a
  // ProcessedStudy object as input instead of a Study object. Remove this field
  // after the update.
  std::set<uint32_t> limited_layers_ids_;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_LIMITED_LAYER_ENTROPY_COST_TRACKER_H_
