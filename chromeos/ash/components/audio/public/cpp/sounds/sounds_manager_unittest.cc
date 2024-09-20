// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/public/cpp/sounds/sounds_manager.h"

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/audio/public/cpp/sounds/audio_stream_handler.h"
#include "chromeos/ash/components/audio/public/cpp/sounds/test_data.h"
#include "media/audio/simple_sources.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_codecs.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace audio {

class SoundsManagerTest : public testing::Test {
 public:
  SoundsManagerTest() = default;
  ~SoundsManagerTest() override = default;

  void SetUp() override {
    SoundsManager::Create(base::DoNothing());
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    SoundsManager::Shutdown();
    base::RunLoop().RunUntilIdle();
  }

  void SetObserverForTesting(AudioStreamHandler::TestObserver* observer) {
    AudioStreamHandler::SetObserverForTesting(observer);
  }

 private:
  base::test::TaskEnvironment env_;
};

TEST_F(SoundsManagerTest, Play) {
  ASSERT_TRUE(SoundsManager::Get());

  base::RunLoop run_loop;
  TestObserver observer(run_loop.QuitClosure());

  SetObserverForTesting(&observer);

  ASSERT_TRUE(SoundsManager::Get()->Initialize(
      kTestAudioKey,
      std::string_view(kTestAudioData, std::size(kTestAudioData)),
      media::AudioCodec::kPCM));
  ASSERT_EQ(20,
            SoundsManager::Get()->GetDuration(kTestAudioKey).InMicroseconds());
  ASSERT_TRUE(SoundsManager::Get()->Play(kTestAudioKey));
  run_loop.Run();

  ASSERT_EQ(1, observer.num_play_requests());
  ASSERT_EQ(1, observer.num_stop_requests());

  SetObserverForTesting(NULL);
}

TEST_F(SoundsManagerTest, Stop) {
  ASSERT_TRUE(SoundsManager::Get());

  base::RunLoop run_loop;
  TestObserver observer(run_loop.QuitClosure());

  SetObserverForTesting(&observer);

  ASSERT_TRUE(SoundsManager::Get()->Initialize(
      kTestAudioKey,
      std::string_view(kTestAudioData, std::size(kTestAudioData)),
      media::AudioCodec::kPCM));

  ASSERT_EQ(0, observer.num_play_requests());
  ASSERT_EQ(0, observer.num_stop_requests());

  ASSERT_TRUE(SoundsManager::Get()->Play(kTestAudioKey));
  ASSERT_TRUE(SoundsManager::Get()->Stop(kTestAudioKey));
  run_loop.Run();

  ASSERT_EQ(1, observer.num_play_requests());
  ASSERT_EQ(1, observer.num_stop_requests());

  SetObserverForTesting(NULL);
}

TEST_F(SoundsManagerTest, Uninitialized) {
  ASSERT_TRUE(SoundsManager::Get());
  ASSERT_FALSE(SoundsManager::Get()->Play(kTestAudioKey));
  ASSERT_FALSE(SoundsManager::Get()->Stop(kTestAudioKey));
}

}  // namespace audio
