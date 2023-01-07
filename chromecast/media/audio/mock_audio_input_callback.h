// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_MOCK_AUDIO_INPUT_CALLBACK_H_
#define CHROMECAST_MEDIA_AUDIO_MOCK_AUDIO_INPUT_CALLBACK_H_

#include "media/audio/audio_io.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromecast {

class MockAudioInputCallback
    : public ::media::AudioInputStream::AudioInputCallback {
 public:
  MockAudioInputCallback();
  ~MockAudioInputCallback() override;

  MOCK_METHOD3(OnData, void(const ::media::AudioBus*, base::TimeTicks, double));
  MOCK_METHOD0(OnError, void());
};

inline MockAudioInputCallback::MockAudioInputCallback() = default;
inline MockAudioInputCallback::~MockAudioInputCallback() = default;

}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_MOCK_AUDIO_INPUT_CALLBACK_H_
