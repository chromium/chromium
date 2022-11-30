// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/reactions/core/reactions_features.h"

namespace content_creation {

BASE_FEATURE(kLightweightReactions,
             "LightweightReactions",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsLightweightReactionsEnabled() {
  return base::FeatureList::IsEnabled(kLightweightReactions);
}

}  // namespace content_creation
