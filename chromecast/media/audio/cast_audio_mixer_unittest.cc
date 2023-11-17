// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/cast_audio_mixer.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromecast/media/api/cma_backend_factory.h"
#include "chromecast/media/audio/cast_audio_manager.h"
#include "chromecast/media/audio/cast_audio_output_stream.h"
#include "chromecast/media/audio/mock_cast_audio_manager_helper_delegate.h"
#include "media/audio/audio_io.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_glitch_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {
namespace {

using testing::_;
using testing::Assign;
using testing::Invoke;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;

// Utility functions
::media::AudioParameters GetAudioParams() {
  return ::media::AudioParameters(
      ::media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      ::media::ChannelLayoutConfig::Stereo(), 48000, 1024);
}

void SignalPull(
    ::media::AudioOutputStream::AudioSourceCallback* source_callback,
    base::TimeDelta delay) {
  std::unique_ptr<::media::AudioBus> audio_bus =
      ::media::AudioBus::Create(GetAudioParams());
  source_callback->OnMoreData(delay, base::TimeTicks::Now(), {},
                              audio_bus.get());
}

void SignalError(
    ::media::AudioOutputStream::AudioSourceCallback* source_callback) {
  source_callback->OnError(
      ::media::AudioOutputStream::AudioSourceCallback::ErrorType::kUnknown);
}

// Mock implementations
class MockAudioSourceCallback
    : public ::media::AudioOutputStream::AudioSourceCallback {
 public:
  MockAudioSourceCallback() {
    ON_CALL(*this, OnMoreData(_, _, _, _))
        .WillByDefault(Invoke(this, &MockAudioSourceCallback::OnMoreDataImpl));
  }

  MOCK_METHOD4(OnMoreData,
               int(base::TimeDelta,
                   base::TimeTicks,
                   const ::media::AudioGlitchInfo&,
                   ::media::AudioBus*));
  MOCK_METHOD1(OnError, void(ErrorType));

 private:
  int OnMoreDataImpl(base::TimeDelta /* delay */,
                     base::TimeTicks /* delay_timestamp */,
                     const ::media::AudioGlitchInfo& /* glitch_info */,
                     ::media::AudioBus* dest) {
    dest->Zero();
    return dest->frames();
  }
};

class MockMediaAudioOutputStream : public ::media::AudioOutputStream {
 public:
  MockMediaAudioOutputStream() {}

  MOCK_METHOD0(Open, bool());
  MOCK_METHOD0(Close, void());
  MOCK_METHOD0(Flush, void());
  MOCK_METHOD1(Start, void(AudioSourceCallback* source_callback));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(SetVolume, void(double volume));
  MOCK_METHOD1(GetVolume, void(double* volume));
};

class MockCastAudioManager : public CastAudioManager {
 public:
  MockCastAudioManager(
      CastAudioManagerHelper::Delegate* delegate,
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner)
      : CastAudioManager(
            std::make_unique<::media::TestAudioThread>(),
            nullptr,
            delegate,
            base::BindRepeating(&MockCastAudioManager::GetCmaBackendFactory,
                                base::Unretained(this)),
            media_task_runner,
            media_task_runner,
            true /* use_mixer */) {
    ON_CALL(*this, ReleaseOutputStream(_))
        .WillByDefault(
            Invoke(this, &MockCastAudioManager::ReleaseOutputStreamConcrete));
  }
  media::CmaBackendFactory* GetCmaBackendFactory() { return nullptr; }

  MOCK_METHOD1(
      MakeMixerOutputStream,
      ::media::AudioOutputStream*(const ::media::AudioParameters& params));
  MOCK_METHOD1(ReleaseOutputStream, void(::media::AudioOutputStream* stream));

 private:
  void ReleaseOutputStreamConcrete(::media::AudioOutputStream* stream) {
    CastAudioManager::ReleaseOutputStream(stream);
  }
};

// Generates StrictMocks of Mixer, Manager, and Mixer OutputStream.
class CastAudioMixerTest : public ::testing::Test {
 public:
  CastAudioMixerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI),
        source_callback_(nullptr) {}
  ~CastAudioMixerTest() override {}

 protected:
  void SetUp() override {
    mock_manager_.reset(new StrictMock<MockCastAudioManager>(
        &delegate_, task_environment_.GetMainThreadTaskRunner()));
    mock_mixer_stream_.reset(new StrictMock<MockMediaAudioOutputStream>());

    ON_CALL(*mock_manager_, MakeMixerOutputStream(_))
        .WillByDefault(Return(mock_mixer_stream_.get()));
    ON_CALL(*mock_mixer_stream_, Start(_))
        .WillByDefault(SaveArg<0>(&source_callback_));
    ON_CALL(*mock_mixer_stream_, Stop())
        .WillByDefault(Assign(&source_callback_, nullptr));
  }

  void TearDown() override { mock_manager_->Shutdown(); }

  MockCastAudioManager& mock_manager() { return *mock_manager_; }
  MockMediaAudioOutputStream& mock_mixer_stream() {
    return *mock_mixer_stream_;
  }

  ::media::AudioOutputStream* CreateMixerStream() {
    return mock_manager_->MakeAudioOutputStream(
        GetAudioParams(), "", ::media::AudioManager::LogCallback());
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockCastAudioManager> mock_manager_;
  std::unique_ptr<MockMediaAudioOutputStream> mock_mixer_stream_;

  // Saved params passed to |mock_mixer_stream_|.
  ::media::AudioOutputStream::AudioSourceCallback* source_callback_;
  MockCastAudioManagerHelperDelegate delegate_;
};

TEST_F(CastAudioMixerTest, Volume) {
  ::media::AudioOutputStream* stream = CreateMixerStream();
  ASSERT_TRUE(stream);

  double volume;
  stream->GetVolume(&volume);
  ASSERT_EQ(volume, 1.0);

  stream->SetVolume(.56);
  stream->GetVolume(&volume);
  ASSERT_EQ(volume, .56);

  EXPECT_CALL(mock_manager(), ReleaseOutputStream(stream));
  stream->Close();
}

TEST_F(CastAudioMixerTest, MixerCallsCloseOnFailedOpen) {
  ::media::AudioOutputStream* stream = CreateMixerStream();
  ASSERT_TRUE(stream);

  EXPECT_CALL(mock_manager(), MakeMixerOutputStream(_))
      .WillOnce(Return(&mock_mixer_stream()));
  EXPECT_CALL(mock_mixer_stream(), Open()).WillOnce(Return(false));
  EXPECT_CALL(mock_mixer_stream(), Close());
  ASSERT_FALSE(stream->Open());

  EXPECT_CALL(mock_manager(), ReleaseOutputStream(stream));
  stream->Close();
}

TEST_F(CastAudioMixerTest, StreamControlOrderMisuse) {
  MockAudioSourceCallback source;
  ::media::AudioOutputStream* stream = CreateMixerStream();
  ASSERT_TRUE(stream);

  // Close stream without first opening
  EXPECT_CALL(mock_manager(), ReleaseOutputStream(stream));
  stream->Close();

  stream = CreateMixerStream();
  ASSERT_TRUE(stream);

  // Should not trigger mixer actions.
  stream->Stop();
  stream->Start(&source);

  EXPECT_CALL(mock_manager(), MakeMixerOutputStream(_))
      .WillOnce(Return(&mock_mixer_stream()));
  EXPECT_CALL(mock_mixer_stream(), Open()).WillOnce(Return(false));
  EXPECT_CALL(mock_mixer_stream(), Close());
  ASSERT_FALSE(stream->Open());

  // Should not trigger mixer actions.
  stream->Start(&source);
  stream->Stop();

  EXPECT_CALL(mock_manager(), MakeMixerOutputStream(_))
      .WillOnce(Return(&mock_mixer_stream()));
  EXPECT_CALL(mock_mixer_stream(), Open()).WillOnce(Return(true));
  ASSERT_TRUE(stream->Open());

  EXPECT_CALL(mock_mixer_stream(), Start(_));
  stream->Start(&source);
  stream->Start(&source);

  EXPECT_CALL(mock_mixer_stream(), Stop());
  EXPECT_CALL(mock_mixer_stream(), Close());
  EXPECT_CALL(mock_manager(), ReleaseOutputStream(stream));
  stream->Close();  // Close abruptly without Stop(), should not fail.
}

TEST_F(CastAudioMixerTest, SingleStreamCycle) {
  MockAudioSourceCallback source;
  ::media::AudioOutputStream* stream = CreateMixerStream();
  ASSERT_TRUE(stream);

  EXPECT_CALL(mock_manager(), MakeMixerOutputStream(_))
      .WillOnce(Return(&mock_mixer_stream()));
  EXPECT_CALL(mock_mixer_stream(), Open()).WillOnce(Return(true));
  ASSERT_TRUE(stream->Open());

  EXPECT_CALL(mock_mixer_stream(), Start(_)).Times(2);
  EXPECT_CALL(mock_mixer_stream(), Stop()).Times(2);
  stream->Start(&source);
  stream->Stop();
  stream->Start(&source);
  stream->Stop();

  EXPECT_CALL(mock_mixer_stream(), Close());
  EXPECT_CALL(mock_manager(), ReleaseOutputStream(stream));
  stream->Close();
}

TEST_F(CastAudioMixerTest, MultiStreamCycle) {
  // This test will break if run with < 1 stream.
  std::vector<::media::AudioOutputStream*> streams(5);
  std::vector<std::unique_ptr<MockAudioSourceCallback>> sources(streams.size());
  std::generate(streams.begin(), streams.end(),
                [this] { return CreateMixerStream(); });
  std::generate(sources.begin(), sources.end(), [] {
    return std::unique_ptr<MockAudioSourceCallback>(
        new StrictMock<MockAudioSourceCallback>());
  });

  EXPECT_CALL(mock_manager(), MakeMixerOutputStream(_))
      .WillOnce(Return(&mock_mixer_stream()));
  EXPECT_CALL(mock_mixer_stream(), Open()).WillOnce(Return(true));
  for (auto* stream : streams)
    ASSERT_TRUE(stream->Open());

  EXPECT_CALL(mock_mixer_stream(), Start(_));
  for (unsigned int i = 0; i < streams.size(); i++)
    streams[i]->Start(sources[i].get());

  // Individually pull out streams
  while (streams.size() > 1) {
    ::media::AudioOutputStream* stream = streams.front();
    stream->Stop();
    streams.erase(streams.begin());
    sources.erase(sources.begin());

    for (auto& source : sources)
      EXPECT_CALL(*source, OnMoreData(_, _, _, _));
    SignalPull(source_callback_, base::TimeDelta());

    EXPECT_CALL(mock_manager(), ReleaseOutputStream(stream));
    stream->Close();
  }

  EXPECT_CALL(mock_mixer_stream(), Stop());
  EXPECT_CALL(mock_mixer_stream(), Close());
  EXPECT_CALL(mock_manager(), ReleaseOutputStream(streams.front()));
  streams.front()->Close();
}

TEST_F(CastAudioMixerTest, TwoStreamRestart) {
  MockAudioSourceCallback source;
  ::media::AudioOutputStream *stream1, *stream2;

  for (int i = 0; i < 2; i++) {
    stream1 = CreateMixerStream();
    stream2 = CreateMixerStream();
    ASSERT_TRUE(stream1);
    ASSERT_TRUE(stream2);

    EXPECT_CALL(mock_manager(), MakeMixerOutputStream(_))
        .WillOnce(Return(&mock_mixer_stream()));
    EXPECT_CALL(mock_mixer_stream(), Open()).WillOnce(Return(true));
    ASSERT_TRUE(stream1->Open());
    ASSERT_TRUE(stream2->Open());

    EXPECT_CALL(mock_mixer_stream(), Start(_));
    stream1->Start(&source);
    stream2->Start(&source);

    stream1->Stop();
    EXPECT_CALL(mock_mixer_stream(), Stop());
    EXPECT_CALL(mock_manager(), ReleaseOutputStream(stream2));
    stream2->Close();
    EXPECT_CALL(mock_mixer_stream(), Close());
    EXPECT_CALL(mock_manager(), ReleaseOutputStream(stream1));
    stream1->Close();
  }
}

TEST_F(CastAudioMixerTest, OnError) {
  MockAudioSourceCallback source;
  std::vector<::media::AudioOutputStream*> streams;

  streams.push_back(CreateMixerStream());
  streams.push_back(CreateMixerStream());
  for (auto* stream : streams)
    ASSERT_TRUE(stream);

  EXPECT_CALL(mock_manager(), MakeMixerOutputStream(_))
      .WillOnce(Return(&mock_mixer_stream()));
  EXPECT_CALL(mock_mixer_stream(), Open()).WillOnce(Return(true));
  for (auto* stream : streams)
    ASSERT_TRUE(stream->Open());

  EXPECT_CALL(mock_mixer_stream(), Start(_));
  streams.front()->Start(&source);

  // Note that error will only be triggered on the first stream because that
  // is the only stream that has been started.
  EXPECT_CALL(source, OnError(_));
  SignalError(source_callback_);
  base::RunLoop().RunUntilIdle();

  // Try to add another stream.
  streams.push_back(CreateMixerStream());
  ASSERT_TRUE(streams.back());
  ASSERT_FALSE(streams.back()->Open());

  EXPECT_CALL(mock_mixer_stream(), Stop());
  EXPECT_CALL(mock_mixer_stream(), Close());
  for (auto* stream : streams) {
    EXPECT_CALL(mock_manager(), ReleaseOutputStream(stream));
    stream->Close();
  }
  streams.clear();

  // Now that the state has been refreshed, attempt to open a stream.
  streams.push_back(CreateMixerStream());
  EXPECT_CALL(mock_manager(), MakeMixerOutputStream(_))
      .WillOnce(Return(&mock_mixer_stream()));
  EXPECT_CALL(mock_mixer_stream(), Open()).WillOnce(Return(true));
  ASSERT_TRUE(streams.front()->Open());

  EXPECT_CALL(mock_mixer_stream(), Start(_));
  streams.front()->Start(&source);

  EXPECT_CALL(mock_mixer_stream(), Stop());
  EXPECT_CALL(mock_mixer_stream(), Close());
  EXPECT_CALL(mock_manager(), ReleaseOutputStream(streams.front()));
  streams.front()->Close();
}

TEST_F(CastAudioMixerTest, Delay) {
  MockAudioSourceCallback source;
  ::media::AudioOutputStream* stream = CreateMixerStream();

  EXPECT_CALL(mock_manager(), MakeMixerOutputStream(_))
      .WillOnce(Return(&mock_mixer_stream()));
  EXPECT_CALL(mock_mixer_stream(), Open()).WillOnce(Return(true));
  ASSERT_TRUE(stream->Open());

  EXPECT_CALL(mock_mixer_stream(), Start(_));
  stream->Start(&source);

  // |delay| is the same because the Mixer and stream are
  // using the same AudioParameters.
  base::TimeDelta delay = base::Microseconds(1000);
  EXPECT_CALL(source, OnMoreData(delay, _, ::media::AudioGlitchInfo(), _));
  SignalPull(source_callback_, delay);

  EXPECT_CALL(mock_mixer_stream(), Stop());
  EXPECT_CALL(mock_mixer_stream(), Close());
  EXPECT_CALL(mock_manager(), ReleaseOutputStream(stream));
  stream->Close();
}

}  // namespace
}  // namespace media
}  // namespace chromecast
