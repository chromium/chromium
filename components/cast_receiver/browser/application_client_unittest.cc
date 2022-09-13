// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/public/application_client.h"

#include "media/base/video_transformation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace cast_receiver {
namespace {

class MockStreamingResolutionObserver : public StreamingResolutionObserver {
 public:
  ~MockStreamingResolutionObserver() override = default;

  MOCK_METHOD2(OnStreamingResolutionChanged,
               void(const gfx::Rect&, const media::VideoTransformation&));
};

class MockApplicationStateObserver : public ApplicationStateObserver {
 public:
  ~MockApplicationStateObserver() override = default;

  MOCK_METHOD1(OnForegroundApplicationChanged,
               void(chromecast::RuntimeApplication*));
};

class ApplicationClientForTesting : public ApplicationClient {
 public:
  ~ApplicationClientForTesting() override = default;

  // Remainder of ApplicationClient implementation.
  MOCK_METHOD0(GetNetworkContextGetter, NetworkContextGetter());
};

}  // namespace

// TODO(crbug.com/1356310): Add tests for ApplicationStateObserver.
class ApplicationClientTest : public testing::Test {
 public:
  ApplicationClientTest() = default;
  ~ApplicationClientTest() override = default;

 protected:
  ApplicationClientForTesting application_client_;

  testing::StrictMock<MockStreamingResolutionObserver>
      first_resolution_observer_;
  testing::StrictMock<MockStreamingResolutionObserver>
      second_resolution_observer_;

  testing::StrictMock<MockApplicationStateObserver> first_state_observer_;
  testing::StrictMock<MockApplicationStateObserver> second_state_observer_;
};

TEST_F(ApplicationClientTest, TestStreamResolutionObservers) {
  gfx::Rect rect;
  media::VideoTransformation video_transformation;
  application_client_.OnStreamingResolutionChanged(rect, video_transformation);

  application_client_.AddStreamingResolutionObserver(
      &first_resolution_observer_);
  EXPECT_CALL(first_resolution_observer_,
              OnStreamingResolutionChanged(testing::_, testing::_));
  application_client_.OnStreamingResolutionChanged(rect, video_transformation);

  application_client_.AddStreamingResolutionObserver(
      &second_resolution_observer_);
  EXPECT_CALL(first_resolution_observer_,
              OnStreamingResolutionChanged(testing::_, testing::_));
  EXPECT_CALL(second_resolution_observer_,
              OnStreamingResolutionChanged(testing::_, testing::_));
  application_client_.OnStreamingResolutionChanged(rect, video_transformation);

  application_client_.RemoveStreamingResolutionObserver(
      &first_resolution_observer_);
  EXPECT_CALL(second_resolution_observer_,
              OnStreamingResolutionChanged(testing::_, testing::_));
  application_client_.OnStreamingResolutionChanged(rect, video_transformation);

  application_client_.RemoveStreamingResolutionObserver(
      &second_resolution_observer_);
  application_client_.OnStreamingResolutionChanged(rect, video_transformation);
}

}  // namespace cast_receiver
