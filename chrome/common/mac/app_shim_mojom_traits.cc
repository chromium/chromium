// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/mac/app_shim_mojom_traits.h"

#include "chrome/common/mac/app_shim.mojom-shared.h"

namespace mojo {

bool StructTraits<chrome::mojom::FeatureStateDataView,
                  variations::VariationsCommandLine>::
    Read(chrome::mojom::FeatureStateDataView data,
         variations::VariationsCommandLine* out) {
  return data.ReadFieldTrialStates(&out->field_trial_states) &&
         data.ReadFieldTrialParams(&out->field_trial_params) &&
         data.ReadEnableFeatures(&out->enable_features) &&
         data.ReadDisableFeatures(&out->disable_features);
}

}  // namespace mojo
