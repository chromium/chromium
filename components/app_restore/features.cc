// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_restore/features.h"

namespace full_restore {
namespace features {

const base::Feature kArcGhostWindow{"ArcGhostWindow",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kArcWindowPredictor{"ArcWindowPredictor",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kFullRestoreForLacros{"FullRestoreForLacros",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

bool IsArcGhostWindowEnabled() {
  return base::FeatureList::IsEnabled(kArcGhostWindow);
}

bool IsArcWindowPredictorEnabled() {
  return base::FeatureList::IsEnabled(kArcWindowPredictor);
}

bool IsFullRestoreForLacrosEnabled() {
  return base::FeatureList::IsEnabled(kFullRestoreForLacros);
}

}  // namespace features
}  // namespace full_restore
