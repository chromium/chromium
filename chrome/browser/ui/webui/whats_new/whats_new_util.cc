// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/whats_new/whats_new_util.h"
#include "base/feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace whats_new {
const char kChromeWhatsNewURL[] = "https://www.google.com/chrome/whats-new/";
const char kChromeWhatsNewURLShort[] = "google.com/chrome/whats-new/";

bool g_is_remote_content_disabled = false;

void DisableRemoteContentForTests() {
  g_is_remote_content_disabled = true;
}

bool IsRemoteContentDisabled() {
  return g_is_remote_content_disabled;
}

bool ShouldShowForState(PrefService* local_state) {
  if (!local_state)
    return false;

  if (!base::FeatureList::IsEnabled(features::kChromeWhatsNewUI))
    return false;

  // Show What's New if the page hasn't yet been shown for the current
  // milestone.
  int last_version = local_state->GetInteger(prefs::kLastWhatsNewVersion);
  return CHROME_VERSION_MAJOR > last_version;
}

void SetLastVersion(PrefService* local_state) {
  if (!local_state) {
    return;
  }

  local_state->SetInteger(prefs::kLastWhatsNewVersion, CHROME_VERSION_MAJOR);
}
}  // namespace whats_new
