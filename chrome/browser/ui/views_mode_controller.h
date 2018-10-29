// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MODE_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_MODE_CONTROLLER_H_

#include "build/build_config.h"
#include "build/buildflag.h"

#if defined(OS_MACOSX)

namespace views_mode_controller {

// Returns whether a Views-capable browser build should use the Cocoa browser
// UI. Always returns false.
// TODO(https://crbug.com/832676): Delete all code guarded on this function
// returning true and then remove this function.
bool IsViewsBrowserCocoa();

}  // namespace views_mode_controller

#endif

#endif  // CHROME_BROWSER_UI_VIEWS_MODE_CONTROLLER_H_
