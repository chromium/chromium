// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/tracking_protection_settings.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"

namespace privacy_sandbox {

void MaybeSetRollbackPrefsModeB(PrefService* prefs) {
  // Only set prefs if user is still in Mode B.
  if (!prefs->GetBoolean(prefs::kTrackingProtection3pcdEnabled)) {
    return;
  }
  const int kBlockThirdParty =
      static_cast<int>(content_settings::CookieControlsMode::kBlockThirdParty);
  bool allowed_3pcs =
      !prefs->GetBoolean(prefs::kBlockAll3pcToggleEnabled) &&
      prefs->GetInteger(prefs::kCookieControlsMode) != kBlockThirdParty;
  if (allowed_3pcs) {
    // If 3PCs are allowed then we should show the notice.
    prefs->SetBoolean(prefs::kShowRollbackUiModeB, allowed_3pcs);
  } else {
    prefs->SetInteger(prefs::kCookieControlsMode, kBlockThirdParty);
  }
  base::UmaHistogramBoolean("Privacy.3PCD.RollbackNotice.ShouldShow",
                            allowed_3pcs);
  prefs->SetBoolean(prefs::kTrackingProtection3pcdEnabled, false);
}

}  // namespace privacy_sandbox
