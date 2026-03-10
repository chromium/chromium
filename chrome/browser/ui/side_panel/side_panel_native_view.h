// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_NATIVE_VIEW_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_NATIVE_VIEW_H_

#include <memory>

#include "build/build_config.h"
#include "ui/base/class_property.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/side_panel/android/side_panel_native_view_android.h"
#else
#include "ui/views/view.h"
#endif

#if BUILDFLAG(IS_ANDROID)
using SidePanelNativeView = std::unique_ptr<SidePanelNativeViewAndroid>;
#else
using SidePanelNativeView = std::unique_ptr<views::View>;
#endif

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_NATIVE_VIEW_H_
