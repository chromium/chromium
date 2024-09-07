// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_HOST_UNITTEST_UTIL_H_
#define CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_HOST_UNITTEST_UTIL_H_

#include "build/build_config.h"

#if defined(TOOLKIT_VIEWS)
void DisableFindBarAnimationsDuringTesting(bool disable);
#else
inline void DisableFindBarAnimationsDuringTesting(bool disable) {}
#endif

#endif  // CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_HOST_UNITTEST_UTIL_H_
