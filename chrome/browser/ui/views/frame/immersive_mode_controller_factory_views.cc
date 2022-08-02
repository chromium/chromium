// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/immersive_mode_controller_stub.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/views/frame/immersive_mode_controller_chromeos.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/views/frame/immersive_mode_controller_mac.h"
#include "chrome/common/chrome_features.h"
#endif

namespace chrome {

std::unique_ptr<ImmersiveModeController> CreateImmersiveModeController() {
#if BUILDFLAG(IS_CHROMEOS)
  return std::make_unique<ImmersiveModeControllerChromeos>();
#elif BUILDFLAG(IS_MAC)
  if (base::FeatureList::IsEnabled(features::kImmersiveFullscreen)) {
    return CreateImmersiveModeControllerMac();
  }
  return std::make_unique<ImmersiveModeControllerStub>();
#else
  return std::make_unique<ImmersiveModeControllerStub>();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace chrome
