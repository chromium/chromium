// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/prefs.h"

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace video_tutorials {

constexpr char kPreferredLocaleKey[] = "video_tutorials.perferred_locale";

constexpr char kLastUpdatedTimeKey[] = "video_tutorials.last_updated_time";

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kPreferredLocaleKey, std::string());
  registry->RegisterTimePref(kLastUpdatedTimeKey, base::Time());
}

}  // namespace video_tutorials
