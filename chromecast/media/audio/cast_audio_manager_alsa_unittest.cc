// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/cast_audio_manager_alsa.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_message_loop.h"
#include "chromecast/media/api/test/mock_cma_backend_factory.h"
#include "chromecast/media/audio/mock_cast_audio_manager_helper_delegate.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/test_audio_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {
namespace {

const char kDefaultAlsaDevice[] = "plug:default";

const ::media::AudioParameters kDefaultAudioParams(
    ::media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
    ::media::ChannelLayoutConfig::Stereo(),
    ::media::AudioParameters::kAudioCDSampleRate,
    256);

void OnLogMessage(const std::string& message) {}

class CastAudioManagerAlsaTest : public testing::Test {
 public:
  CastAudioManagerAlsaTest() : media_thread_("CastMediaThread") {
    CHECK(media_thread_.Start());

    backend_factory_ = std::make_unique<MockCmaBackendFactory>();
    audio_manager_ = std::make_unique<CastAudioManagerAlsa>(
        std::make_unique<::media::TestAudioThread>(), &audio_log_factory_,
        &delegate_,
        base::BindRepeating(&CastAudioManagerAlsaTest::GetCmaBackendFactory,
                            base::Unretained(this)),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        media_thread_.task_runner(), false);
  }

  ~CastAudioManagerAlsaTest() override { audio_manager_->Shutdown(); }
  CmaBackendFactory* GetCmaBackendFactory() { return backend_factory_.get(); }

 protected:
  base::TestMessageLoop message_loop_;
  std::unique_ptr<MockCmaBackendFactory> backend_factory_;
  base::Thread media_thread_;
  ::media::FakeAudioLogFactory audio_log_factory_;
  MockCastAudioManagerHelperDelegate delegate_;
  std::unique_ptr<CastAudioManagerAlsa> audio_manager_;
};

TEST_F(CastAudioManagerAlsaTest, MakeAudioInputStream) {
  ::media::AudioInputStream* stream = audio_manager_->MakeAudioInputStream(
      kDefaultAudioParams, kDefaultAlsaDevice,
      base::BindRepeating(&OnLogMessage));
  ASSERT_TRUE(stream);
  EXPECT_EQ(::media::AudioInputStream::OpenOutcome::kSuccess, stream->Open());
  stream->Close();
}

}  // namespace
}  // namespace media
}  // namespace chromecast
