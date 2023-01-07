// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_UI_UTIL_ANDROID_URL_CONSTANTS_H_
#define COMPONENTS_BROWSER_UI_UTIL_ANDROID_URL_CONSTANTS_H_

#include "build/build_config.h"

namespace browser_ui {
#if BUILDFLAG(IS_ANDROID)
extern const char kChromeUINativeScheme[];
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace browser_ui

#endif  // COMPONENTS_BROWSER_UI_UTIL_ANDROID_URL_CONSTANTS_H_
