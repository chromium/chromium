// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_BROWSER_CONTROL_REMOTING_REMOTING_SESSION_CLIENT_H_
#define COMPONENTS_CAST_STREAMING_BROWSER_CONTROL_REMOTING_REMOTING_SESSION_CLIENT_H_

#include "components/cast_streaming/browser/common/streaming_initialization_info.h"
#include "third_party/openscreen/src/cast/streaming/public/receiver_session.h"

namespace openscreen::cast {
class RpcMessenger;
}  // namespace openscreen::cast

namespace cast_streaming::remoting {

// This class provides an interface for management of a remoting session's
// lifetime events.
class RemotingSessionClient {
 public:
  // This class provides a way for a RemotingSessionClient to start a new
  // streaming session.
  class Dispatcher {
   public:
    virtual ~Dispatcher() = default;

    // Starts a new streaming session with configuration as dictated by
    // |initialization_info|.
    virtual void StartStreamingSession(
        StreamingInitializationInfo initialization_info) = 0;
  };

  virtual ~RemotingSessionClient() = default;

  // Called when a new remoting session is negotiated. |messenger| is the
  // RpcMessenger associated with this session, and is expected to remain valid
  // until either OnRemotingSessionEnded() or this method are called.
  virtual void OnRemotingSessionNegotiated(
      openscreen::cast::RpcMessenger* messenger) = 0;

  // Configures the remoting session using these parameters and upcoming RPC
  // calls received from Openscreen. Will eventually call |dispatcher|'s
  // StartStreamingSession().
  virtual void ConfigureRemotingAsync(
      Dispatcher* dispatcher,
      const openscreen::cast::ReceiverSession* session,
      openscreen::cast::ReceiverSession::ConfiguredReceivers receivers) = 0;

  // Called when a remoting session ends.
  virtual void OnRemotingSessionEnded() = 0;
};

}  // namespace cast_streaming::remoting

#endif  // COMPONENTS_CAST_STREAMING_BROWSER_CONTROL_REMOTING_REMOTING_SESSION_CLIENT_H_
