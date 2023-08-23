// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_RENDERER_WEB_CODECS_STREAM_CONTROL_CHANNEL_H_
#define COMPONENTS_CAST_STREAMING_RENDERER_WEB_CODECS_STREAM_CONTROL_CHANNEL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/cast_streaming/common/public/mojom/demuxer_connector.mojom.h"
#include "components/cast_streaming/renderer/common/buffer_requester.h"
#include "components/cast_streaming/renderer/public/decoder_buffer_provider.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace cast_streaming::webcodecs {

// This class handles synchronization between JavaScript configuration of the
// receiver and the mojo API used for requesting audio and video buffers.
class StreamControlChannel : public mojom::DemuxerConnector,
                             public AudioBufferRequester::Client,
                             public VideoBufferRequester::Client {
 public:
  class Client {
   public:
    virtual ~Client() = default;

    virtual void OnNewAudioBufferProvider(
        base::WeakPtr<AudioDecoderBufferProvider> ptr) = 0;
    virtual void OnNewVideoBufferProvider(
        base::WeakPtr<VideoDecoderBufferProvider> ptr) = 0;
  };

  StreamControlChannel(
      Client* client,
      mojo::PendingAssociatedReceiver<mojom::DemuxerConnector> receiver,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  ~StreamControlChannel() override;

  // To be called when javascript has successfully configured the receiver.
  void OnJavascriptConfigured();

 private:
  // AudioBufferRequester::Client implementation.
  void OnNewBufferProvider(
      base::WeakPtr<AudioDecoderBufferProvider> ptr) override;

  // VideoBufferRequester::Client implementation.
  void OnNewBufferProvider(
      base::WeakPtr<VideoDecoderBufferProvider> ptr) override;

  // mojom::DemuxerConnector implementation.
  void EnableReceiver(EnableReceiverCallback callback) override;
  void OnStreamsInitialized(
      mojom::AudioStreamInitializationInfoPtr audio_stream_info,
      mojom::VideoStreamInitializationInfoPtr video_stream_info) override;

  // Synchronization between the browser-process sender as configured over mojo
  // and the javascript used to display this cast stream. Each of these
  // initializations may occur in either order (or either may not occur in the
  // case where streaming is not initiated), so these variables are needed to
  // correctly synchronize around these two conditions.
  bool has_javascript_been_configured_ = false;
  EnableReceiverCallback enable_receiver_callback_;

  const raw_ptr<Client> client_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::unique_ptr<AudioBufferRequester> audio_buffer_requester_;
  std::unique_ptr<VideoBufferRequester> video_buffer_requester_;
  mojo::AssociatedReceiver<mojom::DemuxerConnector> receiver_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace cast_streaming::webcodecs

#endif  // COMPONENTS_CAST_STREAMING_RENDERER_WEB_CODECS_STREAM_CONTROL_CHANNEL_H_
