// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller_stub.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/views/frame/immersive_mode_controller_chromeos.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller_mac.h"
#include "chrome/browser/ui/window_feature_controller/window_feature_controller.h"
#endif

namespace chrome {

std::unique_ptr<ImmersiveModeController> CreateImmersiveModeController(
    BrowserView* browser_view) {
#if BUILDFLAG(IS_CHROMEOS)
  return std::make_unique<ImmersiveModeControllerChromeos>(
      browser_view->browser());
#elif BUILDFLAG(IS_MAC)
  auto* controller = WindowFeatureController::From(browser_view->browser());
  if (controller->UsesImmersiveFullscreenMode()) {
    return std::make_unique<ImmersiveModeControllerMac>(
        browser_view->browser(),
        /*separate_tab_strip=*/controller->UsesImmersiveFullscreenTabbedMode());
  }
  return std::make_unique<ImmersiveModeControllerStub>(browser_view->browser());
#else
  return std::make_unique<ImmersiveModeControllerStub>(browser_view->browser());
#endif  // BUILDFLAG(IS_CHROMEOS)
}

}  // namespace chrome
