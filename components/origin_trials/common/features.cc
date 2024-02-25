// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_trials/common/features.h"
#include "content/public/common/content_features.h"

namespace origin_trials::features {

bool IsPersistentOriginTrialsEnabled() {
  return base::FeatureList::IsEnabled(::features::kPersistentOriginTrials);
}

}  // namespace origin_trials::features