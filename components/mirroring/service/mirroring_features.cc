// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/mirroring_features.h"

namespace mirroring {
namespace features {
// This flag enables HiDPI capture during Cast Streaming mirroring sessions.
//
// This feature is enabled by the Chrome command line flag
// --enable-cast-streaming-with-hidpi.
BASE_FEATURE(kCastEnableStreamingWithHiDPI, base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
}  // namespace mirroring
