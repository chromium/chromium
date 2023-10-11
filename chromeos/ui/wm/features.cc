// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/wm/features.h"

#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif

namespace chromeos::wm::features {

bool IsWindowLayoutMenuEnabled() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return true;
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()->IsWindowLayoutMenuEnabled();
#else
  return false;
#endif
}

}  // namespace chromeos::wm::features
