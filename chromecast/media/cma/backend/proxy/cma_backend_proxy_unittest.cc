// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/proxy/cma_backend_proxy.h"

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "chromecast/media/api/cma_backend.h"
#include "chromecast/media/api/decoder_buffer_base.h"
#include "chromecast/media/api/test/mock_cma_backend.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;
using testing::StrictMock;

namespace chromecast {
namespace media {
namespace {

class MockMultizoneAudioDecoderProxy : public MultizoneAudioDecoderProxy {
 public:
  MockMultizoneAudioDecoderProxy()
      : MultizoneAudioDecoderProxy(&audio_decoder_) {}
  ~MockMultizoneAudioDecoderProxy() override = default;

  MOCK_METHOD0(Initialize, void());
  MOCK_METHOD1(Start, void(int64_t));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(Pause, void());
  MOCK_METHOD0(Resume, void());
  MOCK_CONST_METHOD0(GetCurrentPts, int64_t());
  MOCK_METHOD1(SetPlaybackRate, void(float rate));
  MOCK_METHOD0(LogicalPause, void());
  MOCK_METHOD0(LogicalResume, void());
  MOCK_METHOD1(SetDelegate, void(Delegate*));
  MOCK_METHOD1(PushBuffer, BufferStatus(scoped_refptr<DecoderBufferBase>));
  MOCK_METHOD1(SetConfig, bool(const AudioConfig&));
  MOCK_METHOD1(SetVolume, bool(float));
  MOCK_METHOD0(GetRenderingDelay, RenderingDelay());
  MOCK_METHOD1(GetStatistics, void(Statistics*));
  MOCK_METHOD0(RequiresDecryption, bool());

 private:
  // Used only for the ctor parameter.
  MockCmaBackend::AudioDecoder audio_decoder_;
};

}  // namespace

class CmaBackendProxyTest : public testing::Test {
 public:
  CmaBackendProxyTest() {
    auto delegated_video_backend =
        std::make_unique<StrictMock<MockCmaBackend>>();
    auto audio_decoder =
        std::make_unique<StrictMock<MockMultizoneAudioDecoderProxy>>();

    delegated_backend_ = delegated_video_backend.get();
    audio_decoder_ = audio_decoder.get();

    CmaBackendProxy::AudioDecoderFactoryCB factory = base::BindOnce(
        [](std::unique_ptr<MultizoneAudioDecoderProxy> ptr) { return ptr; },
        std::move(audio_decoder));
    CmaBackendProxy* proxy = new CmaBackendProxy(
        std::move(factory), std::move(delegated_video_backend));
    backend_.reset(proxy);
  }

 protected:
  void CreateVideoDecoder() {
    EXPECT_CALL(*delegated_backend_, CreateVideoDecoder()).Times(1);
    backend_->CreateVideoDecoder();
    testing::Mock::VerifyAndClearExpectations(delegated_backend_);
  }

  std::unique_ptr<CmaBackendProxy> backend_;
  MockCmaBackend* delegated_backend_;
  MockMultizoneAudioDecoderProxy* audio_decoder_;
};

TEST_F(CmaBackendProxyTest, Initialize) {
  EXPECT_TRUE(backend_->Initialize());

  CreateVideoDecoder();

  EXPECT_CALL(*delegated_backend_, Initialize())
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_TRUE(backend_->Initialize());
  EXPECT_FALSE(backend_->Initialize());
  testing::Mock::VerifyAndClearExpectations(delegated_backend_);

  ASSERT_EQ(backend_->CreateAudioDecoder(), audio_decoder_);
  EXPECT_CALL(*audio_decoder_, Initialize()).Times(2);
  EXPECT_CALL(*delegated_backend_, Initialize())
      .WillOnce(Return(true))
      .WillOnce(Return(false));

  EXPECT_TRUE(backend_->Initialize());
  EXPECT_FALSE(backend_->Initialize());
}

TEST_F(CmaBackendProxyTest, Start) {
  constexpr float kStartPts = 42;
  EXPECT_TRUE(backend_->Start(kStartPts));

  CreateVideoDecoder();

  EXPECT_CALL(*delegated_backend_, Start(kStartPts))
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_TRUE(backend_->Start(kStartPts));
  EXPECT_FALSE(backend_->Start(kStartPts));
  testing::Mock::VerifyAndClearExpectations(delegated_backend_);

  ASSERT_EQ(backend_->CreateAudioDecoder(), audio_decoder_);
  EXPECT_CALL(*audio_decoder_, Start(kStartPts)).Times(2);
  EXPECT_CALL(*delegated_backend_, Start(kStartPts))
      .WillOnce(Return(true))
      .WillOnce(Return(false));

  EXPECT_TRUE(backend_->Start(kStartPts));
  EXPECT_FALSE(backend_->Start(kStartPts));
}

TEST_F(CmaBackendProxyTest, Stop) {
  backend_->Stop();

  CreateVideoDecoder();

  EXPECT_CALL(*delegated_backend_, Stop());
  backend_->Stop();
  testing::Mock::VerifyAndClearExpectations(delegated_backend_);

  ASSERT_EQ(backend_->CreateAudioDecoder(), audio_decoder_);
  EXPECT_CALL(*audio_decoder_, Stop());
  EXPECT_CALL(*delegated_backend_, Stop());
  backend_->Stop();
}

TEST_F(CmaBackendProxyTest, Pause) {
  EXPECT_TRUE(backend_->Pause());

  CreateVideoDecoder();

  EXPECT_CALL(*delegated_backend_, Pause())
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_TRUE(backend_->Pause());
  EXPECT_FALSE(backend_->Pause());
  testing::Mock::VerifyAndClearExpectations(delegated_backend_);

  ASSERT_EQ(backend_->CreateAudioDecoder(), audio_decoder_);
  EXPECT_CALL(*audio_decoder_, Pause()).Times(2);
  EXPECT_CALL(*delegated_backend_, Pause())
      .WillOnce(Return(true))
      .WillOnce(Return(false));

  EXPECT_TRUE(backend_->Pause());
  EXPECT_FALSE(backend_->Pause());
}

TEST_F(CmaBackendProxyTest, Resume) {
  EXPECT_TRUE(backend_->Resume());

  CreateVideoDecoder();

  EXPECT_CALL(*delegated_backend_, Resume())
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_TRUE(backend_->Resume());
  EXPECT_FALSE(backend_->Resume());
  testing::Mock::VerifyAndClearExpectations(delegated_backend_);

  ASSERT_EQ(backend_->CreateAudioDecoder(), audio_decoder_);
  EXPECT_CALL(*audio_decoder_, Resume()).Times(2);
  EXPECT_CALL(*delegated_backend_, Resume())
      .WillOnce(Return(true))
      .WillOnce(Return(false));

  EXPECT_TRUE(backend_->Resume());
  EXPECT_FALSE(backend_->Resume());
}

TEST_F(CmaBackendProxyTest, SetPlaybackRate) {
  constexpr float kSetPlaybackRatePts = 0.5;
  EXPECT_TRUE(backend_->SetPlaybackRate(kSetPlaybackRatePts));

  CreateVideoDecoder();

  EXPECT_CALL(*delegated_backend_, SetPlaybackRate(kSetPlaybackRatePts))
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_TRUE(backend_->SetPlaybackRate(kSetPlaybackRatePts));
  EXPECT_FALSE(backend_->SetPlaybackRate(kSetPlaybackRatePts));
  testing::Mock::VerifyAndClearExpectations(delegated_backend_);

  ASSERT_EQ(backend_->CreateAudioDecoder(), audio_decoder_);
  EXPECT_CALL(*audio_decoder_, SetPlaybackRate(kSetPlaybackRatePts)).Times(2);
  EXPECT_CALL(*delegated_backend_, SetPlaybackRate(kSetPlaybackRatePts))
      .WillOnce(Return(true))
      .WillOnce(Return(false));

  EXPECT_TRUE(backend_->SetPlaybackRate(kSetPlaybackRatePts));
  EXPECT_FALSE(backend_->SetPlaybackRate(kSetPlaybackRatePts));
}

TEST_F(CmaBackendProxyTest, LogicalPause) {
  backend_->LogicalPause();

  CreateVideoDecoder();
  EXPECT_CALL(*delegated_backend_, LogicalPause());
  backend_->LogicalPause();
  testing::Mock::VerifyAndClearExpectations(delegated_backend_);

  ASSERT_EQ(backend_->CreateAudioDecoder(), audio_decoder_);
  EXPECT_CALL(*audio_decoder_, LogicalPause());
  EXPECT_CALL(*delegated_backend_, LogicalPause());
  backend_->LogicalPause();
}

TEST_F(CmaBackendProxyTest, LogicalResume) {
  backend_->LogicalResume();

  CreateVideoDecoder();
  EXPECT_CALL(*delegated_backend_, LogicalResume());
  backend_->LogicalResume();
  testing::Mock::VerifyAndClearExpectations(delegated_backend_);

  ASSERT_EQ(backend_->CreateAudioDecoder(), audio_decoder_);
  EXPECT_CALL(*audio_decoder_, LogicalResume());
  EXPECT_CALL(*delegated_backend_, LogicalResume());
  backend_->LogicalResume();
}

}  // namespace media
}  // namespace chromecast
