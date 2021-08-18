// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/mirroring_features.h"

namespace mirroring {
namespace features {

// Controls whether the Open Screen libcast SenderSession is used for
// initializing and managing streaming sessions, or the legacy implementation.
const base::Feature kOpenscreenCastStreamingSession{
    "OpenscreenCastStreamingSession", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace mirroring
