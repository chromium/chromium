// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/companion/companion_utils.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"

namespace companion {

bool IsSearchWebInCompanionSidePanelSupported(const Browser* browser) {
  if (!browser) {
    return false;
  }
  auto* profile = browser->profile();
  DCHECK(profile);
  return search::DefaultSearchProviderIsGoogle(profile) &&
         !profile->IsOffTheRecord() && browser->is_type_normal() &&
         base::FeatureList::IsEnabled(features::kSidePanelCompanion);
}

}  // namespace companion
