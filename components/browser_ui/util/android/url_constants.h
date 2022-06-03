// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_UI_UTIL_ANDROID_URL_CONSTANTS_H_
#define COMPONENTS_BROWSER_UI_UTIL_ANDROID_URL_CONSTANTS_H_

#include "build/build_config.h"

namespace browser_ui {
#if defined(OS_ANDROID)
extern const char kChromeUINativeScheme[];
#endif  // defined(OS_ANDROID)

}  // namespace browser_ui

#endif  // COMPONENTS_BROWSER_UI_UTIL_ANDROID_URL_CONSTANTS_H_
