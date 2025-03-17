// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_capture/common/content_capture_features.h"

#include "build/build_config.h"

namespace content_capture::features {

constexpr int kTaskInitialDelayMs = 500;

bool IsContentCaptureEnabled() {
#if BUILDFLAG(IS_ANDROID)
  return true;
#else
  return false;
#endif
}

int TaskInitialDelayInMilliseconds() {
  return kTaskInitialDelayMs;
}

}  // namespace content_capture::features
