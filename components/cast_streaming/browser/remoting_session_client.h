// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_BROWSER_REMOTING_SESSION_CLIENT_H_
#define COMPONENTS_CAST_STREAMING_BROWSER_REMOTING_SESSION_CLIENT_H_

namespace openscreen {
namespace cast {
class RpcMessenger;
}  // namespace cast
}  // namespace openscreen

namespace cast_streaming {
namespace remoting {

// This class provides an interface for management of a remoting session's
// lifetime events.
class RemotingSessionClient {
 public:
  virtual ~RemotingSessionClient() = default;

  // Called when a new remoting session is negotiated. |messenger| is the
  // RpcMessenger assocaited with this session, and is expected to remain valid
  // until either OnRemotingSessionEnded() or this method are called.
  virtual void OnRemotingSessionNegotiated(
      openscreen::cast::RpcMessenger* messenger) = 0;

  // Called when a remoting session ends.
  virtual void OnRemotingSessionEnded() = 0;
};

}  // namespace remoting
}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_BROWSER_REMOTING_SESSION_CLIENT_H_
