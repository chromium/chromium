// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_STATUS_BUBBLE_VIEWS_BROWSERTEST_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_STATUS_BUBBLE_VIEWS_BROWSERTEST_MAC_H_

#include "build/build_config.h"
#include "ui/gfx/native_widget_types.h"

namespace test {

#if BUILDFLAG(IS_MAC)
// Returns [window alphaValue]. Widget doesn't offer a GetOpacity(), only
// SetOpacity(). Currently this is only defined for Mac. Obtaining this for
// other platforms is convoluted.
float GetNativeWindowAlphaValue(gfx::NativeWindow window);
#endif

}  // namespace test

#endif  // CHROME_BROWSER_UI_VIEWS_STATUS_BUBBLE_VIEWS_BROWSERTEST_MAC_H_
