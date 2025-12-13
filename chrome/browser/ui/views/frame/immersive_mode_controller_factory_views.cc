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
#endif

namespace chrome {

std::unique_ptr<ImmersiveModeController> CreateImmersiveModeController(
    BrowserView* browser_view) {
#if BUILDFLAG(IS_CHROMEOS)
  return std::make_unique<ImmersiveModeControllerChromeos>(
      browser_view->browser());
#elif BUILDFLAG(IS_MAC)
  if (browser_view->UsesImmersiveFullscreenMode()) {
    return std::make_unique<ImmersiveModeControllerMac>(
        browser_view->browser(),
        /*separate_tab_strip=*/browser_view
            ->UsesImmersiveFullscreenTabbedMode());
  }
  return std::make_unique<ImmersiveModeControllerStub>(browser_view->browser());
#else
  return std::make_unique<ImmersiveModeControllerStub>(browser_view->browser());
#endif  // BUILDFLAG(IS_CHROMEOS)
}

}  // namespace chrome
