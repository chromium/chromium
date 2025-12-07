// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_SPOTLIGHT_AUDIO_STREAM_CONSUMER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_SPOTLIGHT_AUDIO_STREAM_CONSUMER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "remoting/protocol/audio_stub.h"

namespace remoting {
class AudioPacket;
}  // namespace remoting

namespace ash::boca {

// Receives audio packets from a remoting client session and forwards
// them to a callback.
class SpotlightAudioStreamConsumer : public remoting::protocol::AudioStub {
 public:
  // The callback will be invoked for every packet received.
  using AudioPacketReceivedCallback = base::RepeatingCallback<void(
      std::unique_ptr<remoting::AudioPacket> packet)>;

  explicit SpotlightAudioStreamConsumer(AudioPacketReceivedCallback callback);

  SpotlightAudioStreamConsumer(const SpotlightAudioStreamConsumer&) = delete;
  SpotlightAudioStreamConsumer& operator=(const SpotlightAudioStreamConsumer&) =
      delete;

  ~SpotlightAudioStreamConsumer() override;

  // remoting::protocol::AudioStub:
  void ProcessAudioPacket(std::unique_ptr<remoting::AudioPacket> packet,
                          base::OnceClosure done) override;

  base::WeakPtr<SpotlightAudioStreamConsumer> GetWeakPtr();

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  AudioPacketReceivedCallback audio_packet_received_callback_;

  // Needs to be last member variable.
  base::WeakPtrFactory<SpotlightAudioStreamConsumer> weak_factory_{this};
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_SPOTLIGHT_AUDIO_STREAM_CONSUMER_H_
