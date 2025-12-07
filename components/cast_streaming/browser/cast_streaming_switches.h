// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_BROWSER_CAST_STREAMING_SWITCHES_H_
#define COMPONENTS_CAST_STREAMING_BROWSER_CAST_STREAMING_SWITCHES_H_

namespace switches {

// If set, allows to use a specific UDP port for Cast Streaming sessions on the
// receiver side. Otherwise the Cast Streaming receiver is using a random
// system port.
inline constexpr char kCastStreamingReceiverPort[] =
    "cast-streaming-receiver-port";

}  // namespace switches

#endif  // COMPONENTS_CAST_STREAMING_BROWSER_CAST_STREAMING_SWITCHES_H_
