// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_LAYERS_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_LAYERS_H_

#include <map>
#include <optional>

#include "base/component_export.h"
#include "base/metrics/field_trial.h"
#include "base/types/optional_ref.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/processed_study.h"
#include "components/variations/proto/layer.pb.h"
#include "components/variations/proto/variations_seed.pb.h"

namespace variations {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class InvalidLayerReason {
  kInvalidId = 0,
  kNoSlots = 1,
  kNoMembers = 2,
  kInvalidEntropyMode = 3,
  kSlotsDoNotDivideLowEntropyDomain = 4,
  kInvalidSlotBounds = 5,
  kUnknownFields = 6,
  LayerIDNotUnique = 7,
  kLimitedLayerDropped = 8,
  kDuplicatedLayerMemberID = 9,
  kMaxValue = kDuplicatedLayerMemberID,
};

// A view over the layers defined within a variations seed.
//
// A layer defines a collection of mutually exclusive members. For each client,
// at most one member will be assigned as its active member. Studies may be
// conditioned on a particular member being active, in order to avoid overlap
// with studies that require a different member to be active.
class COMPONENT_EXPORT(VARIATIONS) VariationsLayers {
 public:
  // Instantiates a `VariationsLayers` object with the given `seed`, and
  // `entropy_providers`.
  VariationsLayers(const VariationsSeed& seed,
                   const EntropyProviders& entropy_providers);

  VariationsLayers();
  ~VariationsLayers();

  VariationsLayers(const VariationsLayers&) = delete;
  VariationsLayers& operator=(const VariationsLayers&) = delete;

  // True iff the layer members each have valid start and end values, and are
  // non-overlapping. Valid start and end values means that 1) end must be >=
  // start, and 2) they each refer to a slot that's within the range defined in
  // the layer.
  static bool AreSlotBoundsValid(const Layer& layer_proto);

  // True iff a high entropy provider can be used to randomize the study.
  static bool AllowsHighEntropy(const Study& study);

  // Checks whether the layer member reference object is referencing the given
  // `layer_member_id`.
  static bool IsReferencingLayerMemberId(
      const LayerMemberReference& layer_member_reference,
      uint32_t layer_member_id);

  // Returns whether the layer that's associated with the `layer_id` is active.
  // If not, for the same `layer_id`, IsLayerMemberActive() and
  // ActiveLayerMemberDependsOnHighEntropy() will always be false, and
  // GetRemainderEntropy() will return an entropy provider that always
  // randomizes to a fixed value (revealing no entropy).
  bool IsLayerActive(uint32_t layer_id) const;

  // Returns whether any of the layer members referenced are active.
  bool IsLayerMemberActive(
      const LayerMemberReference& layer_member_reference) const;

  // Returns true if the layer has an active member and is configured to use
  // DEFAULT entropy, which means that any study conditioned on it would leak
  // information about the client's high entropy source (including whether or
  // not the client _has_ a high entropy source).
  bool ActiveLayerMemberDependsOnHighEntropy(uint32_t layer_id) const;

  // Returns the entropy provider that should be used to randomize the group
  // assignments of the given study. Or an empty optional if there is no
  // suitable entropy provider. The caller should drop the study upon receiving
  // an empty optional.
  base::optional_ref<const base::FieldTrial::EntropyProvider>
  SelectEntropyProviderForStudy(
      const ProcessedStudy& processed_study,
      const EntropyProviders& entropy_providers) const;

 private:
  struct LayerInfo {
    // Which layer member is active in the layer.
    uint32_t active_member_id;
    // The type of entropy the layer was configured to use.
    Layer::EntropyMode entropy_mode;
    // If this layer has an active member, this is the remaining entropy from
    // that selection, which can be used for uniform randomization of studies
    // conditioned on that layer member.
    // See ComputeRemainderEntropy() for details.
    NormalizedMurmurHashEntropyProvider remainder_entropy;
  };

  void ConstructLayer(const EntropyProviders& entropy_providers,
                      const Layer& layer_proto);

  // Finds the layer with the given `layer_id`. Returns nullptr if there isn't a
  // layer with this id or the layer is invalid.
  const LayerInfo* FindActiveLayer(uint32_t layer_id) const;

  // Gets an EntropyProvider for low entropy randomization of studies
  // conditioned on the layer's active member.
  const base::FieldTrial::EntropyProvider& GetRemainderEntropy(
      uint32_t layer_id) const;

  // Returns the entropy mode of layer with `layer_id`. The optional will not
  // have a value if there isn't a layer with this id, the layer is invalid, or
  // the layer is not active.
  std::optional<Layer::EntropyMode> GetEntropyMode(uint32_t layer_id) const;

  NormalizedMurmurHashEntropyProvider nil_entropy;
  std::map<uint32_t, LayerInfo> active_member_for_layer_;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_LAYERS_H_
