// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_MIXER_SERVICE_CONNECTION_H_
#define CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_MIXER_SERVICE_CONNECTION_H_

#include <cstdint>
#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chromecast/media/audio/mixer_service/mixer_service.pb.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace net {
class StreamSocket;
}  // namespace net

namespace chromecast {
namespace media {

class MixerServiceConnection {
 public:
  class Delegate {
   public:
    virtual void FillNextBuffer(void* buffer,
                                int frames,
                                int64_t playout_timestamp) = 0;
    virtual void OnConnectionError() = 0;
    virtual void OnEosPlayed() = 0;

   protected:
    virtual ~Delegate() = default;
  };

  MixerServiceConnection(Delegate* delegate,
                         const mixer_service::MixerStreamParams& params);
  ~MixerServiceConnection();

  void Connect();
  void SendNextBuffer(int filled_frames);
  void SetVolumeMultiplier(float multiplier);

 private:
  class Socket;

  void ConnectCallback(int result);
  void ConnectTimeout();

  Delegate* const delegate_;
  const mixer_service::MixerStreamParams params_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::unique_ptr<net::StreamSocket> connecting_socket_;
  std::unique_ptr<Socket> socket_;
  float volume_multiplier_ = 1.0f;

  base::WeakPtrFactory<MixerServiceConnection> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MixerServiceConnection);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_MIXER_SERVICE_CONNECTION_H_
