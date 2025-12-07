// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/spotlight/spotlight_audio_stream_consumer.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/protocol/frame_consumer.h"

namespace ash::boca {

SpotlightAudioStreamConsumer::SpotlightAudioStreamConsumer(
    AudioPacketReceivedCallback callback)
    : audio_packet_received_callback_(std::move(callback)) {}

SpotlightAudioStreamConsumer::~SpotlightAudioStreamConsumer() = default;

void SpotlightAudioStreamConsumer::ProcessAudioPacket(
    std::unique_ptr<remoting::AudioPacket> packet,
    base::OnceClosure done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  audio_packet_received_callback_.Run(std::move(packet));
  std::move(done).Run();
}

base::WeakPtr<SpotlightAudioStreamConsumer>
SpotlightAudioStreamConsumer::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace ash::boca
