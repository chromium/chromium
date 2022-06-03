// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_search/side_search_utils.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/side_search/side_search_prefs.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/prefs/pref_service.h"

bool IsSideSearchEnabled(const Profile* profile) {
  return !profile->IsOffTheRecord() &&
         base::FeatureList::IsEnabled(features::kSideSearch) &&
         profile->GetPrefs()->GetBoolean(side_search_prefs::kSideSearchEnabled);
}
