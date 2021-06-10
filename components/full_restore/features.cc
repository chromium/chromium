// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/full_restore/features.h"

namespace full_restore {
namespace features {

const base::Feature kArcGhostWindow{"ArcGhostWindow",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kFullRestore{"FullRestore",
                                 base::FEATURE_ENABLED_BY_DEFAULT};

bool IsArcGhostWindowEnabled() {
  return IsFullRestoreEnabled() &&
         base::FeatureList::IsEnabled(kArcGhostWindow);
}

bool IsFullRestoreEnabled() {
  return base::FeatureList::IsEnabled(kFullRestore);
}

}  // namespace features
}  // namespace full_restore
