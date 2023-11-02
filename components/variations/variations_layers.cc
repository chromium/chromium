// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_layers.h"

#include "base/metrics/field_trial.h"

namespace variations {

namespace {

// Iterates through the members of the given layer proto definition, and
// returns the member which contains that slot (if any).
const Layer::LayerMember* FindActiveMemberBySlot(uint32_t chosen_slot,
                                                 const Layer& layer_proto) {
  for (const Layer::LayerMember& member : layer_proto.members()) {
    if (!member.id())
      continue;

    for (const Layer::LayerMember::SlotRange& slot : member.slots()) {
      if (slot.start() <= chosen_slot && chosen_slot <= slot.end())
        return &member;
    }
  }

  return nullptr;
}

}  // namespace

VariationsLayers::VariationsLayers(const VariationsSeed& seed,
                                   const EntropyProviders& entropy_providers) {
  // TODO(crbug.com/1154033): Support a way to expire old/unused layers so they
  // no longer get processed by the clients.
  for (const Layer& layer_proto : seed.layers())
    ConstructLayer(entropy_providers, layer_proto);
}

VariationsLayers::VariationsLayers() = default;

VariationsLayers::~VariationsLayers() = default;

void VariationsLayers::ConstructLayer(const EntropyProviders& entropy_providers,
                                      const Layer& layer_proto) {
  if (layer_proto.id() == 0 || layer_proto.num_slots() == 0 ||
      layer_proto.members_size() == 0) {
    return;
  }

  if (layer_proto.entropy_mode() != Layer::LOW &&
      layer_proto.entropy_mode() != Layer::DEFAULT) {
    return;
  }

  const auto& entropy_provider = (layer_proto.entropy_mode() != Layer::LOW)
                                     ? entropy_providers.default_entropy()
                                     : entropy_providers.low_entropy();

  uint32_t chosen_slot = entropy_provider.GetPseudorandomValue(
      layer_proto.salt(), layer_proto.num_slots());

  const auto* chosen_member = FindActiveMemberBySlot(chosen_slot, layer_proto);
  if (!chosen_member)
    return;
  active_member_for_layer_.emplace(
      layer_proto.id(), LayerInfo{.active_member_id = chosen_member->id(),
                                  .entropy_mode = layer_proto.entropy_mode()});
}

bool VariationsLayers::IsLayerMemberActive(uint32_t layer_id,
                                           uint32_t member_id) const {
  auto layer_iter = active_member_for_layer_.find(layer_id);
  if (layer_iter == active_member_for_layer_.end())
    return false;

  return layer_iter->second.active_member_id &&
         (member_id == layer_iter->second.active_member_id);
}

bool VariationsLayers::ActiveLayerMemberDependsOnHighEntropy(
    uint32_t layer_id) const {
  auto layer_iter = active_member_for_layer_.find(layer_id);
  if (layer_iter == active_member_for_layer_.end())
    return false;

  return layer_iter->second.entropy_mode == Layer::DEFAULT;
}

}  // namespace variations
