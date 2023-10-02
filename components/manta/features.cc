// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/features.h"

#include "base/feature_list.h"

namespace manta::features {

BASE_FEATURE(kMantaService, "MantaService", base::FEATURE_DISABLED_BY_DEFAULT);

bool IsMantaServiceEnabled() {
  return base::FeatureList::IsEnabled(kMantaService);
}

}  // namespace manta::features
