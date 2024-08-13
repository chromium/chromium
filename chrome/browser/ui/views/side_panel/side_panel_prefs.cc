// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_prefs.h"
#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/ui/views/side_panel/companion/companion_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"

namespace side_panel_prefs {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  // When in RTL mode, the side panel should default to the left of the screen.
  // Otherwise, the side panel should default to the right side of the screen.
  // TODO(dljames): Add enum values kAlternateSide / kDefaultSide that will
  // replace false and true respectively.
  registry->RegisterBooleanPref(prefs::kSidePanelHorizontalAlignment,
                                !base::i18n::IsRTL());
  if (companion::IsCompanionFeatureEnabled()) {
    registry->RegisterBooleanPref(
        prefs::kSidePanelCompanionEntryPinnedToToolbar,
        base::FeatureList::IsEnabled(
            features::kSidePanelCompanionDefaultPinned),
        user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  }
  registry->RegisterBooleanPref(prefs::kGoogleSearchSidePanelEnabled, true);
  registry->RegisterDictionaryPref(prefs::kSidePanelIdToWidth);
}

}  // namespace side_panel_prefs
