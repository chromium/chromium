// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_MOCK_CAST_AUDIO_MANAGER_HELPER_DELEGATE_H_
#define CHROMECAST_MEDIA_AUDIO_MOCK_CAST_AUDIO_MANAGER_HELPER_DELEGATE_H_

#include "chromecast/media/audio/cast_audio_manager_helper.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromecast {
namespace media {

class MockCastAudioManagerHelperDelegate
    : public CastAudioManagerHelper::Delegate {
 public:
  MockCastAudioManagerHelperDelegate();
  ~MockCastAudioManagerHelperDelegate() override;

  MOCK_METHOD1(GetSessionId, std::string(const std::string&));
  MOCK_METHOD1(IsAudioOnlySession, bool(const std::string&));
  MOCK_METHOD1(IsGroup, bool(const std::string&));
};

inline MockCastAudioManagerHelperDelegate::
    MockCastAudioManagerHelperDelegate() = default;
inline MockCastAudioManagerHelperDelegate::
    ~MockCastAudioManagerHelperDelegate() = default;

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_MOCK_CAST_AUDIO_MANAGER_HELPER_DELEGATE_H_
