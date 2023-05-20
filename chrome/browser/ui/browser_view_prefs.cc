// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_view_prefs.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace {

#if BUILDFLAG(IS_LINUX)
bool GetCustomFramePrefDefault() {
#if BUILDFLAG(IS_OZONE)
  return ui::OzonePlatform::GetInstance()
      ->GetPlatformProperties()
      .custom_frame_pref_default;
#else
  return false;
#endif  // BUILDFLAG(IS_OZONE)
}
#endif

}  // namespace

void RegisterBrowserViewProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
#if BUILDFLAG(IS_LINUX)
  registry->RegisterBooleanPref(prefs::kUseCustomChromeFrame,
                                GetCustomFramePrefDefault());
#endif
}
