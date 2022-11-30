// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_ui/util/android/url_constants.h"

#include "build/build_config.h"

namespace browser_ui {
#if BUILDFLAG(IS_ANDROID)
const char kChromeUINativeScheme[] = "chrome-native";
#endif

}  // namespace browser_ui
