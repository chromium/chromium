// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_preview_data_service.h"

#include "components/prefs/pref_registry_simple.h"
#include "components/signin/public/base/signin_pref_names.h"

namespace signin {

// static
void AccountPreviewDataService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterTimePref(prefs::kAccountPreviewDataLastUpdatePref,
                             base::Time());
}

}  // namespace signin
