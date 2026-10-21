// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_DIRECT_LAUNCH_DIRECT_LAUNCH_FEATURES_H_
#define CHROME_BROWSER_WIN_DIRECT_LAUNCH_DIRECT_LAUNCH_FEATURES_H_

#include "base/feature_list.h"

namespace direct_launch {

// When enabled on Windows 11 and later, the browser registers with the shell
// so that URI and Start menu tile launches are delivered straight to the
// running browser process, rather than by starting a new browser process that
// only hands the launch over to the running one.
BASE_DECLARE_FEATURE(kDirectLaunchManager);

}  // namespace direct_launch

#endif  // CHROME_BROWSER_WIN_DIRECT_LAUNCH_DIRECT_LAUNCH_FEATURES_H_
