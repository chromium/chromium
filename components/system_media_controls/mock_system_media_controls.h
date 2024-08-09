// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYSTEM_MEDIA_CONTROLS_MOCK_SYSTEM_MEDIA_CONTROLS_H_
#define COMPONENTS_SYSTEM_MEDIA_CONTROLS_MOCK_SYSTEM_MEDIA_CONTROLS_H_

#include "components/system_media_controls/system_media_controls.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace system_media_controls {

class SystemMediaControlsObserver;

namespace testing {

// Mock implementation of SystemMediaControls for testing.
class MockSystemMediaControls : public SystemMediaControls {
 public:
  MockSystemMediaControls();

  MockSystemMediaControls(const MockSystemMediaControls&) = delete;
  MockSystemMediaControls& operator=(const MockSystemMediaControls&) = delete;

  ~MockSystemMediaControls() override;

  // SystemMediaControls implementation.
  MOCK_METHOD1(AddObserver, void(SystemMediaControlsObserver* observer));
  MOCK_METHOD1(RemoveObserver, void(SystemMediaControlsObserver* observer));
  MOCK_METHOD1(SetEnabled, void(bool enabled));
  MOCK_METHOD1(SetIsNextEnabled, void(bool value));
  MOCK_METHOD1(SetIsPreviousEnabled, void(bool value));
  MOCK_METHOD1(SetIsPlayPauseEnabled, void(bool value));
  MOCK_METHOD1(SetIsStopEnabled, void(bool value));
  MOCK_METHOD1(SetIsSeekToEnabled, void(bool value));
  MOCK_METHOD1(SetPlaybackStatus, void(PlaybackStatus value));
  MOCK_METHOD1(SetID, void(const std::string* value));
  MOCK_METHOD1(SetTitle, void(const std::u16string& title));
  MOCK_METHOD1(SetArtist, void(const std::u16string& artist));
  MOCK_METHOD1(SetAlbum, void(const std::u16string& artist));
  MOCK_METHOD1(SetThumbnail, void(const SkBitmap& bitmap));
  MOCK_METHOD1(SetPosition, void(const media_session::MediaPosition& position));
  MOCK_METHOD0(ClearThumbnail, void());
  MOCK_METHOD0(ClearMetadata, void());
  MOCK_METHOD0(UpdateDisplay, void());
  MOCK_CONST_METHOD0(GetVisibilityForTesting, bool());
  MOCK_METHOD1(SetOnBridgeCreatedCallbackForTesting,
               void(base::RepeatingCallback<void()>));
};

}  // namespace testing

}  // namespace system_media_controls

#endif  // COMPONENTS_SYSTEM_MEDIA_CONTROLS_MOCK_SYSTEM_MEDIA_CONTROLS_H_
