// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromecast/media/cma/backend/alsa/alsa_volume_control.h"

#include <memory>

#include "base/test/task_environment.h"
#include "chromecast/media/cma/backend/system_volume_control.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

namespace {

class MockDelegate : public SystemVolumeControl::Delegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;

  MOCK_METHOD(void,
              OnSystemVolumeOrMuteChange,
              (float new_volume, bool new_mute),
              (override));
};

TEST(AlsaVolumeControlTest, Construct) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);
  MockDelegate delegate;
  AlsaVolumeControl alsa_volume_control(
      &delegate, std::make_unique<::media::AlsaWrapper>());
}

}  // namespace

}  // namespace media
}  // namespace chromecast
