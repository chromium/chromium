// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/utils.h"

#include "build/build_config.h"
#include "components/saved_tab_groups/types.h"

namespace tab_groups {

bool AreLocalIdsPersisted() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return true;
#else
  return false;
#endif
}

}  // namespace tab_groups
