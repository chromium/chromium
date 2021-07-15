// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_STREAMING_RECEIVER_SESSION_CLIENT_H_
#define CHROMECAST_CAST_CORE_STREAMING_RECEIVER_SESSION_CLIENT_H_

#include <memory>

#include "chromecast/browser/cast_web_contents.h"
#include "components/cast/message_port/message_port.h"
#include "components/cast_streaming/browser/public/receiver_session.h"

namespace chromecast {

// This class wraps all //components/cast_streaming functionality, only
// expecting the caller to supply a MessagePortFactory. Internally, it
// manages the lifetimes of cast streaming objects, and informs the caller
// of important events.
class StreamingReceiverSessionClient
    : public CastWebContents::Observer,
      public cast_api_bindings::MessagePort::Receiver {
 public:
  class Handler {
   public:
    virtual ~Handler();

    //.Called when the streaming session as successfully been initialized,
    // following navigation of the observed CastWebContents to a cast-supporting
    // URL.
    virtual void OnStreamingSessionStarted() = 0;

    // Called when a fatal error occurs.
    virtual void OnError() = 0;

    // Called when an AV settings query must be started for |message_port|.
    virtual void StartAvSettingsQuery(
        std::unique_ptr<cast_api_bindings::MessagePort> message_port) = 0;
  };

  // Creates a new instance of this class. |handler| must persist for the
  // lifetime of this instance.
  StreamingReceiverSessionClient(
      cast_streaming::ReceiverSession::MessagePortProvider
          message_port_provider,
      Handler* handler);
  ~StreamingReceiverSessionClient() override;

  // Starts the Streaming Receiver owned by this instance. May only be called
  // once. At time of calling, this instance will be set as the observer of
  // |cast_web_contents|, for which streaming will be started upon navigation to
  // an associated URL. Following this call, the supported AV Settings are
  // expected to remain constant.
  void LaunchStreamingReceiver(CastWebContents* cast_web_contents);

  bool has_streaming_started() const { return has_streaming_started_; }

 private:
  friend class StreamingReceiverSessionClientTest;

  using ReceiverSessionFactory =
      base::OnceCallback<std::unique_ptr<cast_streaming::ReceiverSession>(
          cast_streaming::ReceiverSession::AVConstraints)>;

  // This second ctor is required for Unit Testing.
  StreamingReceiverSessionClient(ReceiverSessionFactory factory,
                                 Handler* handler);

  // CastWebContents::Observer overrides.
  void MainFrameReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;

  // cast_api_bindings::MessagePort::Receiver overrides.
  bool OnMessage(base::StringPiece message,
                 std::vector<std::unique_ptr<cast_api_bindings::MessagePort>>
                     ports) override;
  void OnPipeError() override;

  // Handler for callbacks associated with this class. May be empty.
  Handler* const handler_;

  // Most recently received AV Constraints, from bindings.
  absl::optional<cast_streaming::ReceiverSession::AVConstraints>
      av_constraints_;

  // Responsible for managing the streaming session.
  std::unique_ptr<cast_streaming::ReceiverSession> receiver_session_;

  // MessagePort responsible for receiving AV Settings Bindings Messages
  std::unique_ptr<cast_api_bindings::MessagePort> message_port_;

  // Factory method used to create a receiver session.
  ReceiverSessionFactory receiver_session_factory_;

  bool has_streaming_started_ = false;
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_STREAMING_RECEIVER_SESSION_CLIENT_H_
