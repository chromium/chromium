// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_BROWSER_STREAMING_CONTROLLER_H_
#define COMPONENTS_CAST_RECEIVER_BROWSER_STREAMING_CONTROLLER_H_

#include "base/functional/callback.h"
#include "components/cast_streaming/browser/public/receiver_config.h"
#include "components/cast_streaming/browser/public/receiver_session.h"

namespace cast_receiver {

// This class is responsible for configuring a Cast Streaming session and
// beginning its playback.
class StreamingController {
 public:
  using PlaybackStartedCB = base::OnceCallback<void()>;

  virtual ~StreamingController() = default;

  // Creates a new cast_streaming::ReceiverSession to use for this streaming
  // session.
  virtual void InitializeReceiverSession(
      cast_streaming::ReceiverConfig config,
      cast_streaming::ReceiverSession::Client* client) = 0;

  // Begins playback once all preconditions have been met, at which time |cb| is
  // called.
  virtual void StartPlaybackAsync(PlaybackStartedCB cb) = 0;
};

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_BROWSER_STREAMING_CONTROLLER_H_
