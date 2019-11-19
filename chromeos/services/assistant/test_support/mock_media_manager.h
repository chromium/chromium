// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_TEST_SUPPORT_MOCK_MEDIA_MANAGER_H_
#define CHROMEOS_SERVICES_ASSISTANT_TEST_SUPPORT_MOCK_MEDIA_MANAGER_H_

#include "libassistant/shared/public/media_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {
namespace assistant {

class MockMediaManager : public assistant_client::MediaManager {
 public:
  MockMediaManager();
  ~MockMediaManager() override;

  // MediaManager:
  MOCK_METHOD1(AddListener, void(Listener*));
  MOCK_METHOD0(Next, void());
  MOCK_METHOD0(Previous, void());
  MOCK_METHOD0(Pause, void());
  MOCK_METHOD0(Resume, void());
  MOCK_METHOD0(StopAndClearPlaylist, void());
  MOCK_METHOD0(PlayPause, void());
  MOCK_METHOD1(SetExternalPlaybackState,
               void(const assistant_client::MediaStatus&));
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_TEST_SUPPORT_MOCK_MEDIA_MANAGER_H_
