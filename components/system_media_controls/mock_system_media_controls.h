// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYSTEM_MEDIA_CONTROLS_MOCK_SYSTEM_MEDIA_CONTROLS_H_
#define COMPONENTS_SYSTEM_MEDIA_CONTROLS_MOCK_SYSTEM_MEDIA_CONTROLS_H_

#include "base/macros.h"
#include "components/system_media_controls/system_media_controls.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace system_media_controls {

class SystemMediaControlsObserver;

namespace testing {

// Mock implementation of SystemMediaControls for testing.
class MockSystemMediaControls : public SystemMediaControls {
 public:
  MockSystemMediaControls();
  ~MockSystemMediaControls() override;

  // SystemMediaControls implementation.
  MOCK_METHOD1(AddObserver, void(SystemMediaControlsObserver* observer));
  MOCK_METHOD1(RemoveObserver, void(SystemMediaControlsObserver* observer));
  MOCK_METHOD1(SetEnabled, void(bool enabled));
  MOCK_METHOD1(SetIsNextEnabled, void(bool value));
  MOCK_METHOD1(SetIsPreviousEnabled, void(bool value));
  MOCK_METHOD1(SetIsPlayPauseEnabled, void(bool value));
  MOCK_METHOD1(SetIsStopEnabled, void(bool value));
  MOCK_METHOD1(SetPlaybackStatus, void(PlaybackStatus value));
  MOCK_METHOD1(SetTitle, void(const base::string16& title));
  MOCK_METHOD1(SetArtist, void(const base::string16& artist));
  MOCK_METHOD1(SetAlbum, void(const base::string16& artist));
  MOCK_METHOD1(SetThumbnail, void(const SkBitmap& bitmap));
  MOCK_METHOD0(ClearThumbnail, void());
  MOCK_METHOD0(ClearMetadata, void());
  MOCK_METHOD0(UpdateDisplay, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSystemMediaControls);
};

}  // namespace testing

}  // namespace system_media_controls

#endif  // COMPONENTS_SYSTEM_MEDIA_CONTROLS_MOCK_SYSTEM_MEDIA_CONTROLS_H_
