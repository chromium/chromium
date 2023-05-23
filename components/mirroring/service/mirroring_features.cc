// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/mirroring_features.h"

namespace mirroring {
namespace features {

// The mirroring sender has the ability to letterbox video frames to match the
// aspect ratio of the reciever's display.  However, receivers can handle
// variable aspect ratio video so this is not needed any more.
// TODO(crbug.com/1363512):  Remove support for sender side letterboxing.
BASE_FEATURE(kCastDisableLetterboxing,
             "CastDisableLetterboxing",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The mirroring service previously used a model name filter before even
// attempting to query the receiver for media remoting support. This
// flag disables this behavior and queries all devices for remoting support.
// See https://crbug.com/1198616 and b/224993260 for background.
BASE_FEATURE(kCastDisableModelNameCheck,
             "CastDisableModelNameCheck",
             base::FEATURE_ENABLED_BY_DEFAULT);

// This flag enables HiDPI capture during Cast Streaming mirroring sessions.
//
// This feature is enabled by the Chrome command line flag
// --enable-cast-streaming-with-hidpi.
BASE_FEATURE(kCastEnableStreamingWithHiDPI,
             "CastEnableStreamingWithHiDPI",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
}  // namespace mirroring
