// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/side_panel_ui_provider.h"

#include "build/build_config.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/side_panel/internal/android/side_panel_coordinator_android.h"
#else
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#endif

// static
SidePanelUI* SidePanelUIProvider::From(BrowserWindowInterface* browser) {
#if BUILDFLAG(IS_ANDROID)
  return SidePanelCoordinatorAndroid::From(browser);
#else
  return SidePanelCoordinator::From(browser);
#endif
}
