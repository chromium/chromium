// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/features.h"

#include "base/feature_list.h"
#include "chrome/browser/buildflags.h"

namespace tabs {

BASE_FEATURE(kTabSearchPositionSetting,
             "TabSearchPositionSetting",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool CanShowTabSearchPositionSetting() {
// Mac and other platforms will always have the tab search position in the
// correct location, cros/linux/win git the user the option to change.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
  return base::FeatureList::IsEnabled(kTabSearchPositionSetting);
#else
  return false;
#endif
}

}  // namespace tabs
