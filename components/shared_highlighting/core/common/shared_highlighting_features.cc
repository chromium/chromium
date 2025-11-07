// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/shared_highlighting/core/common/shared_highlighting_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace shared_highlighting {

int GetPreemptiveLinkGenTimeoutLengthMs() {
#if BUILDFLAG(IS_ANDROID)
  return 100;
#else
  return 500;
#endif
}

}  // namespace shared_highlighting
