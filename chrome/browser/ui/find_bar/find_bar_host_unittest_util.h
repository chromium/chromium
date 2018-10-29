// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_HOST_UNITTEST_UTIL_H_
#define CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_HOST_UNITTEST_UTIL_H_

#include "build/build_config.h"

namespace chrome {

#if defined(TOOLKIT_VIEWS)
void DisableFindBarAnimationsDuringTesting(bool disable);
#else
static inline void DisableFindBarAnimationsDuringTesting(bool disable) {}
#endif

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_HOST_UNITTEST_UTIL_H_
