// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_layers.h"

#include "base/metrics/field_trial.h"

namespace variations {

namespace {

// Returns a double in the [0, 1] range to be used to select a slot within
// a layer with the given randomization seed value.
double GetEntropyForLayer(
    const base::FieldTrial::EntropyProvider* entropy_provider,
    uint32_t randomization_seed) {
  // GetEntropyForTrial will ignore the trial_name parameter in favor of the
  // randomization_seed.
  return entropy_provider->GetEntropyForTrial(/*trial_name=*/"",
                                              randomization_seed);
}

// Iterates through the members of the given layer proto definition, and
// returns the ID of the member which contains that slot (if any).
base::Optional<uint32_t> FindActiveMemberBySlot(uint32_t chosen_slot,
                                                const Layer& layer_proto) {
  for (const Layer::LayerMember& member : layer_proto.members()) {
    if (!member.id())
      continue;

    for (const Layer::LayerMember::SlotRange& slot : member.slots()) {
      if (slot.start() <= chosen_slot && chosen_slot <= slot.end())
        return member.id();
    }
  }

  return base::nullopt;
}

}  // namespace

VariationsLayers::LayerInfo::LayerInfo(
    base::Optional<uint32_t> active_member_id,
    Layer::EntropyMode entropy_mode)
    : active_member_id(active_member_id), entropy_mode(entropy_mode) {}

VariationsLayers::LayerInfo::~LayerInfo() = default;

VariationsLayers::LayerInfo::LayerInfo(const LayerInfo& other) {
  active_member_id = other.active_member_id;
  entropy_mode = other.entropy_mode;
}

VariationsLayers::VariationsLayers(
    const VariationsSeed& seed,
    const base::FieldTrial::EntropyProvider* low_entropy_provider) {
  if (!low_entropy_provider) {
    // Android WebView does not support low-entropy field trials.
    return;
  }

  // TODO(crbug.com/1154033): Support a way to expire old/unused layers so they
  // no longer get processed by the clients.
  for (const Layer& layer_proto : seed.layers())
    ConstructLayer(*low_entropy_provider, layer_proto);
}

VariationsLayers::VariationsLayers() = default;

VariationsLayers::~VariationsLayers() = default;

void VariationsLayers::ConstructLayer(
    const base::FieldTrial::EntropyProvider& low_entropy_provider,
    const Layer& layer_proto) {
  if (layer_proto.id() == 0 || layer_proto.num_slots() == 0 ||
      layer_proto.members_size() == 0) {
    return;
  }

  double entropy_value;
  if (layer_proto.entropy_mode() == Layer::LOW) {
    entropy_value =
        GetEntropyForLayer(&low_entropy_provider, layer_proto.salt());
  } else {
    const base::FieldTrial::EntropyProvider* default_entropy_provider =
        base::FieldTrialList::GetEntropyProviderForOneTimeRandomization();
    CHECK(default_entropy_provider);
    entropy_value =
        GetEntropyForLayer(default_entropy_provider, layer_proto.salt());
  }

  const double kEpsilon = 1e-8;
  // Add a tiny epsilon to get consistent values when converting the double
  // to the integer slots; see comment in
  // base::FieldTrialList::GetGroupBoundaryValue() for more details.
  uint32_t chosen_slot = std::min(
      static_cast<uint32_t>(layer_proto.num_slots() * entropy_value + kEpsilon),
      layer_proto.num_slots() - 1);

  active_member_for_layer_.emplace(
      layer_proto.id(),
      LayerInfo{FindActiveMemberBySlot(chosen_slot, layer_proto),
                layer_proto.entropy_mode()});
}

bool VariationsLayers::IsLayerMemberActive(uint32_t layer_id,
                                           uint32_t member_id) const {
  auto layer_iter = active_member_for_layer_.find(layer_id);
  if (layer_iter == active_member_for_layer_.end())
    return false;

  return layer_iter->second.active_member_id &&
         (member_id == layer_iter->second.active_member_id.value());
}

bool VariationsLayers::IsLayerUsingDefaultEntropy(uint32_t layer_id) const {
  auto layer_iter = active_member_for_layer_.find(layer_id);
  if (layer_iter == active_member_for_layer_.end())
    return false;

  return layer_iter->second.entropy_mode == Layer::DEFAULT;
}

}  // namespace variations
