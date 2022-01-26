// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_STREAMING_CONTROLLER_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_STREAMING_CONTROLLER_H_

#include <memory>

#include "base/callback.h"
#include "components/cast_streaming/browser/public/receiver_session.h"

namespace chromecast {

// This class is responsible for configuring a Cast Streaming session and
// beginning its playback.
class StreamingController {
 public:
  using PlaybackStartedCB = base::OnceCallback<void()>;

  virtual ~StreamingController() = default;

  // Creates a new cast_streaming::ReceiverSession to use for this streaming
  // session.
  virtual void InitializeReceiverSession(
      std::unique_ptr<cast_streaming::ReceiverSession::AVConstraints>
          constraints,
      cast_streaming::ReceiverSession::Client* client) = 0;

  // Begins playback once all preconditions have been met, at which time |cb| is
  // called.
  virtual void StartPlaybackAsync(PlaybackStartedCB cb) = 0;
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_STREAMING_CONTROLLER_H_
