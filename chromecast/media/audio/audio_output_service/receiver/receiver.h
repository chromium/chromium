// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_AUDIO_OUTPUT_SERVICE_RECEIVER_RECEIVER_H_
#define CHROMECAST_MEDIA_AUDIO_AUDIO_OUTPUT_SERVICE_RECEIVER_RECEIVER_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chromecast/media/audio/net/audio_socket_service.h"

namespace chromecast {
namespace media {
namespace audio_output_service {
class Generic;
class OutputSocket;

class Receiver : public AudioSocketService::Delegate {
 public:
  Receiver(const std::string& uds_path, int tcp_port);
  Receiver(const Receiver&) = delete;
  Receiver& operator=(const Receiver&) = delete;
  ~Receiver() override;

  virtual void CreateOutputStream(std::unique_ptr<OutputSocket> socket,
                                  const Generic& message) = 0;

 private:
  class InitialSocket;

  // mixer_service::AudioSocketService::Delegate implementation:
  void HandleAcceptedSocket(std::unique_ptr<net::StreamSocket> socket) override;

  void AddInitialSocket(std::unique_ptr<InitialSocket> initial_socket);
  void RemoveInitialSocket(InitialSocket* initial_socket);

  AudioSocketService socket_service_;
  base::flat_map<InitialSocket*, std::unique_ptr<InitialSocket>>
      initial_sockets_;
};

}  // namespace audio_output_service
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_AUDIO_OUTPUT_SERVICE_RECEIVER_RECEIVER_H_
