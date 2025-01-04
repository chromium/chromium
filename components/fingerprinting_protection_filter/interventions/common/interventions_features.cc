// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/interventions/common/interventions_features.h"

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"

namespace fingerprinting_protection_interventions::features {

bool IsCanvasInterventionsFeatureEnabled() {
  return base::FeatureList::IsEnabled(blink::features::kCanvasInterventions);
}

// TODO(crbug.com/380458351): Add incognito-specific feature enabled accessor.
// TODO(crbug.com/380463018): Add base::FeatureParams for signatures.

}  // namespace fingerprinting_protection_interventions::features
