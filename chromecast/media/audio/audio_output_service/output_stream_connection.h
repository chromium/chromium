// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_AUDIO_OUTPUT_SERVICE_OUTPUT_STREAM_CONNECTION_H_
#define CHROMECAST_MEDIA_AUDIO_AUDIO_OUTPUT_SERVICE_OUTPUT_STREAM_CONNECTION_H_

#include <cstdint>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "chromecast/common/mojom/audio_socket.mojom.h"
#include "chromecast/media/audio/audio_output_service/audio_output_service.pb.h"
#include "chromecast/media/audio/audio_output_service/output_connection.h"
#include "chromecast/media/audio/audio_output_service/output_socket.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace net {
class IOBuffer;
}  // namespace net

namespace chromecast {
namespace media {
namespace audio_output_service {

// OutputStreamConnection sends proto messages and audio buffers through the
// underlying socket to the audio output service to control the audio decoder
// and render audio. It also receives status updates from the audio output
// service. Threading model: this class should be created and used on an IO
// thread.
class OutputStreamConnection : public OutputConnection,
                               public OutputSocket::Delegate {
 public:
  // Delegate methods will be called on the sequence OutputStreamConnection is
  // created.
  class Delegate {
   public:
    // Called when the audio pipeline backend is initialized.
    virtual void OnBackendInitialized(
        const BackendInitializationStatus& status) = 0;

    // Called when the audio pipeline backend is ready to receive the next
    // buffer.
    // TODO(b/173250111): Remove `media_timestamp_microseconds` and
    // `reference_timestamp_microseconds` once all the implementations switched
    // to using delay information.
    virtual void OnNextBuffer(int64_t media_timestamp_microseconds,
                              int64_t reference_timestamp_microseconds,
                              int64_t delay_microseconds,
                              int64_t delay_timestamp_microseconds) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  OutputStreamConnection(
      Delegate* delegate,
      CmaBackendParams params,
      mojo::PendingRemote<mojom::AudioSocketBroker> pending_socket_broker);
  OutputStreamConnection(const OutputStreamConnection&) = delete;
  OutputStreamConnection& operator=(const OutputStreamConnection&) = delete;
  ~OutputStreamConnection() override;

  // Connects to the audio output service. After this is called, delegate
  // methods may start to be called. If the output connection is lost, this will
  // automatically reconnect.
  void Connect();

  // Sends |audio_buffer| to the audio output service.
  void SendAudioBuffer(scoped_refptr<net::IOBuffer> audio_buffer,
                       int buffer_size_bytes,
                       int64_t pts);

  // Starts the playback from |start_pts|.
  void StartPlayingFrom(int64_t start_pts);

  // Stops the playback.
  void StopPlayback();

  // Sets the playback rate.
  void SetPlaybackRate(float playback_rate);

  // Sets the playback volume.
  void SetVolume(float volume);

  // Updates the config for the audio decoder.
  void UpdateAudioConfig(const CmaBackendParams& params);

 private:
  // OutputConnection implementation:
  void OnConnected(std::unique_ptr<OutputSocket> socket) override;
  void OnConnectionFailed() override;
  void OnConnectionError() override;

  // OutputSocket::Delegate implementation:
  bool HandleMetadata(const Generic& message) override;

  void SendHeartbeat();

  Delegate* const delegate_;
  CmaBackendParams params_;
  std::unique_ptr<OutputSocket> socket_;
  base::OneShotTimer heartbeat_timer_;
  float volume_ = 1.0f;
  float playback_rate_ = 1.0f;
  bool sent_eos_ = false;
  bool dropping_audio_ = false;
};

}  // namespace audio_output_service
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_AUDIO_OUTPUT_SERVICE_OUTPUT_STREAM_CONNECTION_H_
