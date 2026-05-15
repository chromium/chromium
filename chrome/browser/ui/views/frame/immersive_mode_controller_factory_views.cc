// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller_stub.h"
#include "chrome/browser/ui/window_feature_controller/window_feature_controller.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/views/frame/immersive_mode_controller_chromeos.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/views/frame/immersive_mode_controller_mac.h"
#include "chrome/browser/ui/window_feature_controller/window_feature_controller.h"
#endif

namespace chrome {

std::unique_ptr<ImmersiveModeController> CreateImmersiveModeController(
    WindowFeatureController* window_feature_controller,
    ui::UnownedUserDataHost& host) {
#if BUILDFLAG(IS_CHROMEOS)
  return std::make_unique<ImmersiveModeControllerChromeos>(host);
#elif BUILDFLAG(IS_MAC)
  if (window_feature_controller->UsesImmersiveFullscreenMode()) {
    return std::make_unique<ImmersiveModeControllerMac>(
        host,
        /*separate_tab_strip=*/window_feature_controller
            ->UsesImmersiveFullscreenTabbedMode());
  }
  return std::make_unique<ImmersiveModeControllerStub>(host);
#else
  return std::make_unique<ImmersiveModeControllerStub>(host);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

}  // namespace chrome
