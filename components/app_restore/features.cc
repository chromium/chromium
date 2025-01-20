// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_restore/features.h"

namespace full_restore {
namespace features {

BASE_FEATURE(kArcWindowPredictor,
             "ArcWindowPredictor",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsArcWindowPredictorEnabled() {
  return base::FeatureList::IsEnabled(kArcWindowPredictor);
}

}  // namespace features
}  // namespace full_restore
