// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_action/page_action_icon_type.h"

#include "base/feature_list.h"
#include "chrome/browser/ui/ui_features.h"

namespace {

const base::FeatureParam<bool>* GetPageActionMigrationParam(
    PageActionIconType page_action) {
  switch (page_action) {
    case PageActionIconType::kLensOverlay:
      return &features::kPageActionsMigrationLensOverlay;
    case PageActionIconType::kMemorySaver:
      return &features::kPageActionsMigrationMemorySaver;
    case PageActionIconType::kTranslate:
      return &features::kPageActionsMigrationTranslate;
    case PageActionIconType::kIntentPicker:
      return &features::kPageActionsMigrationIntentPicker;
    case PageActionIconType::kZoom:
      return &features::kPageActionsMigrationZoom;
    case PageActionIconType::kPaymentsOfferNotification:
      return &features::kPageActionsMigrationOfferNotification;
    case PageActionIconType::kFileSystemAccess:
      return &features::kPageActionsMigrationFileSystemAccess;
    case PageActionIconType::kPwaInstall:
      return &features::kPageActionsMigrationPwaInstall;
    case PageActionIconType::kPriceInsights:
      return &features::kPageActionsMigrationPriceInsights;
    case PageActionIconType::kManagePasswords:
      return &features::kPageActionsMigrationManagePasswords;
    default:
      return nullptr;
  }
}

}  // namespace

bool IsPageActionMigrated(PageActionIconType page_action) {
  const auto* feature_param = GetPageActionMigrationParam(page_action);
  if (feature_param == nullptr) {
    return false;
  }

  // For developer manual testing only, allow all migrated page actions to be
  // enabled through a single switch.
  if (features::kPageActionsMigrationEnableAll.Get()) {
    return true;
  }

  return feature_param->Get();
}
