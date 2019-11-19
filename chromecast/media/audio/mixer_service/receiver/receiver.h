// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_RECEIVER_RECEIVER_H_
#define CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_RECEIVER_RECEIVER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chromecast/media/audio/mixer_service/audio_socket_service.h"
#include "chromecast/media/audio/mixer_service/mixer_service.pb.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace chromecast {
namespace media {
namespace mixer_service {
class MixerSocket;

class Receiver : public AudioSocketService::Delegate {
 public:
  Receiver();
  ~Receiver() override;

  virtual void CreateOutputStream(std::unique_ptr<MixerSocket> socket,
                                  const Generic& message) = 0;
  virtual void CreateLoopbackConnection(std::unique_ptr<MixerSocket> socket,
                                        const Generic& message) = 0;
  virtual void CreateAudioRedirection(std::unique_ptr<MixerSocket> socket,
                                      const Generic& message) = 0;
  virtual void CreateControlConnection(std::unique_ptr<MixerSocket> socket,
                                       const Generic& message) = 0;

  // Creates a local (in-process) connection to this receiver. May be called on
  // any thread; the returned MixerSocket can only be used on the calling
  // thread. The returned socket must have its delegate set immediately.
  std::unique_ptr<MixerSocket> LocalConnect();

 private:
  class InitialSocket;

  // AudioSocketService::Delegate implementation:
  void HandleAcceptedSocket(std::unique_ptr<net::StreamSocket> socket) override;

  void HandleLocalConnection(std::unique_ptr<MixerSocket> socket);

  void AddInitialSocket(std::unique_ptr<InitialSocket> initial_socket);
  void RemoveInitialSocket(InitialSocket* socket);

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  AudioSocketService socket_service_;

  base::flat_map<InitialSocket*, std::unique_ptr<InitialSocket>>
      initial_sockets_;

  base::WeakPtrFactory<Receiver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Receiver);
};

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_RECEIVER_RECEIVER_H_
