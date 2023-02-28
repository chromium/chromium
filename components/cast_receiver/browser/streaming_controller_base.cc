// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/streaming_controller_base.h"

#include "base/functional/bind.h"
#include "components/cast/message_port/message_port.h"
#include "components/cast/message_port/platform_message_port.h"
#include "components/cast_receiver/browser/streaming_controller_mirroring.h"
#include "components/cast_receiver/browser/streaming_controller_remoting.h"
#include "components/cast_streaming/browser/public/receiver_session.h"
#include "components/cast_streaming/common/public/features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace cast_receiver {

// static
std::unique_ptr<StreamingController> StreamingControllerBase::Create(
    std::unique_ptr<cast_api_bindings::MessagePort> message_port,
    content::WebContents* web_contents) {
  if (cast_streaming::IsCastRemotingEnabled()) {
    return std::make_unique<StreamingControllerRemoting>(
        std::move(message_port), web_contents);
  }

  return std::make_unique<StreamingControllerMirroring>(std::move(message_port),
                                                        web_contents);
}

StreamingControllerBase::StreamingControllerBase(
    std::unique_ptr<cast_api_bindings::MessagePort> message_port,
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      message_port_(std::move(message_port)) {
  DCHECK(message_port_);
}

StreamingControllerBase::~StreamingControllerBase() = default;

void StreamingControllerBase::ProcessConfig(
    cast_streaming::ReceiverConfig& config) {}

void StreamingControllerBase::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(navigation_handle);

  navigation_handle->GetRenderFrameHost()
      ->GetRemoteAssociatedInterfaces()
      ->GetInterface(&demuxer_connector_);
  navigation_handle->GetRenderFrameHost()
      ->GetRemoteAssociatedInterfaces()
      ->GetInterface(&renderer_connection_);

  DCHECK(demuxer_connector_);
  DCHECK(renderer_connection_);

  TryStartPlayback();
}

void StreamingControllerBase::InitializeReceiverSession(
    cast_streaming::ReceiverConfig config,
    cast_streaming::ReceiverSession::Client* client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!client_);

  config_.emplace(std::move(config));
  client_ = client;

  ProcessConfig(*config_);

  TryStartPlayback();
}

void StreamingControllerBase::StartPlaybackAsync(PlaybackStartedCB cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!playback_started_cb_);
  DCHECK(cb);

  playback_started_cb_ = std::move(cb);

  TryStartPlayback();
}

void StreamingControllerBase::TryStartPlayback() {
  if (playback_started_cb_ && demuxer_connector_ && config_) {
    cast_streaming::ReceiverSession::MessagePortProvider message_port_provider =
        base::BindOnce(
            [](std::unique_ptr<cast_api_bindings::MessagePort> port) {
              return port;
            },
            std::move(message_port_));
    receiver_session_ = cast_streaming::ReceiverSession::Create(
        *config_, std::move(message_port_provider), client_);
    DCHECK(receiver_session_);

    StartPlayback(receiver_session_.get(), std::move(demuxer_connector_),
                  std::move(renderer_connection_));
    std::move(playback_started_cb_).Run();
  }
}

}  // namespace cast_receiver
