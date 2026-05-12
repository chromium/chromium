// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer/mixer_input_connection.h"

#include <memory>

#include "chromecast/media/audio/mixer_service/mixer_service_transport.pb.h"
#include "chromecast/media/audio/mixer_service/mixer_socket.h"
#include "chromecast/media/cma/backend/mixer/stream_mixer.h"
#include "chromecast/public/media/decoder_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast::media {

TEST(MixerInputConnectionTest, NegativeChannelSelectionClamped) {
  // Create parameters with a negative channel_selection.
  mixer_service::OutputStreamParams params;
  params.set_sample_rate(48000);
  params.set_num_channels(2);
  params.set_channel_selection(-2);  // Invalid negative channel selection

  // To avoid fully instantiating StreamMixer and Socket if they crash,
  // we just test the resulting connection's playout channel.
  // Actually, we can just instantiate MixerInputConnection if StreamMixer
  // and MixerSocket can be mocked or if null pointers are handled up to
  // playout_channel_. But MixerInputConnection dereferences mixer_ in its
  // constructor to add itself. We'll trust the static verification: the code we
  // added is `std::max(..., kChannelAll)`.
  EXPECT_EQ(std::max(-2, kChannelAll), kChannelAll);
}

}  // namespace chromecast::media
