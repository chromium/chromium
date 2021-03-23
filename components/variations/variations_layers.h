// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_LAYERS_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_LAYERS_H_

#include <map>

#include "base/metrics/field_trial.h"
#include "base/optional.h"
#include "components/variations/proto/variations_seed.pb.h"

namespace variations {

// Parses out the Variations Layers data from the provided seed and
// chooses which member within each layer should be the active one.
class VariationsLayers {
 public:
  VariationsLayers(
      const VariationsSeed& seed,
      const base::FieldTrial::EntropyProvider* low_entropy_provider);

  VariationsLayers();
  ~VariationsLayers();

  VariationsLayers(const VariationsLayers&) = delete;
  VariationsLayers& operator=(const VariationsLayers&) = delete;

  // Return whether the given layer has the given member active.
  bool IsLayerMemberActive(uint32_t layer_id, uint32_t member_id) const;

  // Return whether the layer should use the default entropy source
  // (usually the high entropy source).
  bool IsLayerUsingDefaultEntropy(uint32_t layer_id) const;

 private:
  void ConstructLayer(
      const base::FieldTrial::EntropyProvider& low_entropy_provider,
      const Layer& layer_proto);

  struct LayerInfo {
    LayerInfo(base::Optional<uint32_t> active_member_id,
              Layer::EntropyMode entropy_mode);
    ~LayerInfo();
    LayerInfo(const LayerInfo& other);  // = delete;
    LayerInfo& operator=(const LayerInfo&) = delete;

    base::Optional<uint32_t> active_member_id;
    Layer::EntropyMode entropy_mode;
  };
  std::map<uint32_t, LayerInfo> active_member_for_layer_;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_LAYERS_H_
