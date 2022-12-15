// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_LAYERS_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_LAYERS_H_

#include <map>

#include "base/component_export.h"
#include "base/metrics/field_trial.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace variations {

enum class InvalidLayerReason {
  kInvalidId = 0,
  kNoSlots = 1,
  kNoMembers = 2,
  kInvalidEntropyMode = 3,
  kSlotsDoNotDivideLowEntropyDomain = 4,
  kInvalidSlotBounds = 5,
  kUnknownFields = 6,
  kMaxValue = kUnknownFields,
};

// A view over the layers defined within a variations seed.
//
// A layer defines a collection of mutually exclusive members. For each client,
// at most one member will be assigned as its active member. Studies may be
// conditioned on a particular member being active, in order to avoid overlap
// with studies that require a different member to be active.
class COMPONENT_EXPORT(VARIATIONS) VariationsLayers {
 public:
  VariationsLayers(const VariationsSeed& seed,
                   const EntropyProviders& entropy_providers);

  VariationsLayers();
  ~VariationsLayers();

  VariationsLayers(const VariationsLayers&) = delete;
  VariationsLayers& operator=(const VariationsLayers&) = delete;

  // Returns whether the given layer has the given member active.
  bool IsLayerMemberActive(uint32_t layer_id, uint32_t member_id) const;

  // Returns true if the layer has an active member and is configured to use
  // DEFAULT entropy, which means that any study conditioned on it would leak
  // information about the client's high entropy source (including whether or
  // not the client _has_ a high entropy source).
  bool ActiveLayerMemberDependsOnHighEntropy(uint32_t layer_id) const;

  // Gets an EntropyProvider for low entropy randomization of studies
  // conditioned on the layer's active member.
  const base::FieldTrial::EntropyProvider& GetRemainderEntropy(
      uint32_t layer_id) const;

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

  NormalizedMurmurHashEntropyProvider nil_entropy;
  std::map<uint32_t, LayerInfo> active_member_for_layer_;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_LAYERS_H_
