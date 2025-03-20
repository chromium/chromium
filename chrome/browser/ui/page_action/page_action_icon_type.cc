// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_action/page_action_icon_type.h"

#include "base/feature_list.h"
#include "chrome/browser/ui/ui_features.h"

bool IsPageActionMigrated(PageActionIconType page_action) {
  if (!base::FeatureList::IsEnabled(features::kPageActionsMigration)) {
    return false;
  }
  switch (page_action) {
    case PageActionIconType::kLensOverlay:
      return features::kPageActionsMigrationLensOverlay.Get();
    case PageActionIconType::kMemorySaver:
      return features::kPageActionsMigrationMemorySaver.Get();
    case PageActionIconType::kTranslate:
      return features::kPageActionsMigrationTranslate.Get();
    case PageActionIconType::kIntentPicker:
      return features::kPageActionsMigrationIntentPicker.Get();
    case PageActionIconType::kZoom:
      return features::kPageActionsMigrationZoom.Get();
    case PageActionIconType::kPaymentsOfferNotification:
      return features::kPageActionsMigrationOfferNotification.Get();
    default:
      return false;
  }
}
