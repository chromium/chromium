// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/mixer_service/redirected_audio_connection.h"

#include <vector>

#include "chromecast/media/audio/mixer_service/mock_mixer_socket.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {
namespace mixer_service {

namespace {

using ::testing::_;
using ::testing::Return;

// Copied from redirected_audio_connection.cc
enum MessageTypes : int {
  kRedirectionRequest = 1,
  kStreamMatchPatterns,
};

class MockRedirectedAudioObserver
    : public mixer_service::RedirectedAudioConnection::Delegate {
 public:
  MockRedirectedAudioObserver() {
    ON_CALL(*this, OnRedirectedAudio(_, _, _))
        .WillByDefault(::testing::Invoke(
            this, &MockRedirectedAudioObserver::OnRedirectedAudioImpl));
  }

  MOCK_METHOD(void, OnRedirectedAudio, (int64_t, float*, int), (override));
  MOCK_METHOD(void, SetSampleRate, (int), (override));

 private:
  void OnRedirectedAudioImpl(int64_t timestamp, float* data, int frames) {
    data_.clear();
    // Save received data to local.
    data_.insert(data_.end(), data, data + frames);
  }

  std::vector<float> data_;
};

class RedirectedAudioConnectionTest : public ::testing::Test {
 public:
  RedirectedAudioConnectionTest() {}
};

TEST_F(RedirectedAudioConnectionTest, EmptyConfig) {
  RedirectedAudioConnection::Config config;
  MockRedirectedAudioObserver observer;
  std::unique_ptr<MockMixerSocket> socket = std::make_unique<MockMixerSocket>();
  MockMixerSocket* socket_ptr = socket.get();

  RedirectedAudioConnection connection(config, &observer);

  EXPECT_CALL(*socket_ptr, SetDelegate(&connection));
  EXPECT_CALL(*socket_ptr, SendProto(MessageTypes::kRedirectionRequest, _));
  connection.ConnectForTest(std::move(socket));
}

}  // namespace

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast
