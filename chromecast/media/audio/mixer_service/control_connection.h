// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_CONTROL_CONNECTION_H_
#define CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_CONTROL_CONNECTION_H_

#include <list>
#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "chromecast/media/audio/mixer_service/mixer_connection.h"
#include "chromecast/media/audio/mixer_service/mixer_socket.h"
#include "chromecast/public/volume_control.h"

namespace chromecast {
namespace media {
namespace mixer_service {

// Mixer service connection for controlling general mixer properties, such as
// device volume and postprocessor configuration. Not thread-safe; all usage of
// a given instance must be on the same sequence. Must be created on an IO
// thread.
class ControlConnection : public MixerConnection, public MixerSocket::Delegate {
 public:
  using ConnectedCallback = base::RepeatingClosure;

  // Callback to receive mixer stream count changes.
  using StreamCountCallback =
      base::RepeatingCallback<void(int primary_streams, int sfx_streams)>;

  // Callback that handles ListPostProcessors response.
  using ListPostprocessorsCallback =
      base::OnceCallback<void(const std::vector<std::string>&)>;

  ControlConnection();

  ControlConnection(const ControlConnection&) = delete;
  ControlConnection& operator=(const ControlConnection&) = delete;

  ~ControlConnection() override;

  // Connects to the mixer. If the mixer connection is lost, this will
  // automatically reconnect. If |callback| is nonempty, it will be called each
  // time a connection is (re)established with the mixer. This can be used to
  // re-send postprocessor messages, since they are not persisted across
  // disconnects.
  void Connect(ConnectedCallback callback = ConnectedCallback());

  // Sets volume multiplier for all streams of a given content type.
  void SetVolume(AudioContentType type, float volume_multiplier);

  // Sets mute state all streams of a given content type.
  void SetMuted(AudioContentType type, bool muted);

  // Sets the maximum effective volume multiplier for a given content type.
  void SetVolumeLimit(AudioContentType type, float max_volume_multiplier);

  // Returns a set of registered builtin post-processors.
  void ListPostprocessors(ListPostprocessorsCallback callback);

  // Sends arbitrary config data to a specific postprocessor. Config is saved
  // for each unique |name| and will be resent if the mixer disconnects and then
  // reconnects. If the |postprocessor_name| contains a '?', that character and
  // the remainder of the name string will not be sent to the mixer; this is
  // useful for configuring multiple subprocessors (eg for the dynamic range
  // processor).
  void ConfigurePostprocessor(std::string postprocessor_name,
                              std::string config);

  // Sends a message a specific postprocessor. Messages are not saved and will
  // not be resent if the mixer disconnects and then reconnects.
  void SendPostprocessorMessage(std::string postprocessor_name,
                                std::string message);

  // Instructs the mixer to reload postprocessors based on the config file.
  void ReloadPostprocessors();

  // Sets a callback to receive mixer stream count changes. |callback| may be an
  // empty callback to remove it.
  void SetStreamCountCallback(StreamCountCallback callback);

  // Sets the desired number of output channels used by the mixer. This will
  // cause an audio interruption on any currently active streams. The actual
  // output channel count is determined by the output implementation and may not
  // match |num_channels|.
  void SetNumOutputChannels(int num_channels);

 private:
  bool SendPostprocessorMessageInternal(std::string postprocessor_name,
                                        std::string message);
  void OnSendFailed();

  // MixerConnection implementation:
  void OnConnected(std::unique_ptr<MixerSocket> socket) override;
  void OnConnectionError() override;

  // MixerSocket::Delegate implementation:
  bool HandleMetadata(const Generic& message) override;

  std::unique_ptr<MixerSocket> socket_;

  ConnectedCallback connect_callback_;

  base::flat_map<AudioContentType, float> volume_;
  base::flat_map<AudioContentType, bool> muted_;
  base::flat_map<AudioContentType, float> volume_limit_;

  base::flat_map<std::string, std::string> postprocessor_config_;

  StreamCountCallback stream_count_callback_;
  // Uses std::list to trigger callbacks in FIFO order.
  std::list<ListPostprocessorsCallback> list_postprocessors_callbacks_;
  int num_output_channels_ = 0;
};

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_CONTROL_CONNECTION_H_
