// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/common/public/features.h"

#include "components/cast_streaming/buildflags.h"

namespace cast_streaming {

bool IsCastRemotingEnabled() {
#if BUILDFLAG(CAST_STREAMING_ENABLE_REMOTING)
  return true;
#else   // BUILDFLAG(CAST_STREAMING_ENABLE_REMOTING)
  return false;
#endif  // BUILDFLAG(CAST_STREAMING_ENABLE_REMOTING)
}

}  // namespace cast_streaming
