// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_API_TEST_MOCK_CAST_SOUNDS_MANAGER_H_
#define CHROMECAST_MEDIA_API_TEST_MOCK_CAST_SOUNDS_MANAGER_H_

#include <string>

#include "base/functional/callback.h"
#include "chromecast/media/api/cast_sounds_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromecast {
namespace media {

class MockCastSoundsManager : public CastSoundsManager {
 public:
  MockCastSoundsManager();
  ~MockCastSoundsManager() override;
  MOCK_METHOD4(AddSound, void(int, int, bool, bool));
  MOCK_METHOD4(AddSoundWithAudioData, void(int, const std::string, bool, bool));
  MOCK_METHOD2(Play, void(int, AudioContentType));
  MOCK_METHOD1(Stop, void(int));
  void GetDuration(int key, DurationCallback callback) override {
    DoGetDuration(key, &callback);
  }
  MOCK_METHOD2(DoGetDuration, void(int, DurationCallback*));
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_API_TEST_MOCK_CAST_SOUNDS_MANAGER_H_
