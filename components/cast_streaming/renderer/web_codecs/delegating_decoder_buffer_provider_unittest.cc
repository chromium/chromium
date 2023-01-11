// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/renderer/web_codecs/delegating_decoder_buffer_provider.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/cast_streaming/renderer/public/decoder_buffer_provider.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cast_streaming::webcodecs {
namespace {

class MockDecoderBufferProvider
    : public DecoderBufferProvider<media::AudioDecoderConfig> {
 public:
  using NewBufferCb =
      typename DecoderBufferProvider<media::AudioDecoderConfig>::NewBufferCb;
  using GetConfigCb =
      typename DecoderBufferProvider<media::AudioDecoderConfig>::GetConfigCb;
  using DeletionCb =
      typename DecoderBufferProvider<media::AudioDecoderConfig>::DeletionCb;

  MockDecoderBufferProvider() : weak_factory_(this) {}
  ~MockDecoderBufferProvider() override = default;

  MOCK_CONST_METHOD0(IsValid, bool());
  MOCK_CONST_METHOD1(GetConfigAsync, void(GetConfigCb));
  MOCK_METHOD1(ReadBufferAsync, void(NewBufferCb));
  MOCK_METHOD1(SetInvalidationCallback, void(DeletionCb));

  base::WeakPtrFactory<MockDecoderBufferProvider> weak_factory_;
};

class MockClient {
 public:
  MOCK_METHOD1(OnConfig, void(media::AudioDecoderConfig));
  MOCK_METHOD1(OnBuffer, void(scoped_refptr<media::DecoderBuffer>));
  MOCK_METHOD0(OnDeletion, void());
};

}  // namespace

class DelegatingDecoderBufferProviderTest : public testing::Test {
 public:
  DelegatingDecoderBufferProviderTest()
      : provider_impl_(
            std::make_unique<testing::StrictMock<MockDecoderBufferProvider>>()),
        provider_(std::make_unique<
                  DelegatingDecoderBufferProvider<media::AudioDecoderConfig>>(
            provider_impl_->weak_factory_.GetWeakPtr(),
            task_environment_.GetMainThreadTaskRunner())) {}

  ~DelegatingDecoderBufferProviderTest() override = default;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<testing::StrictMock<MockDecoderBufferProvider>>
      provider_impl_;
  testing::StrictMock<MockClient> client_;
  std::unique_ptr<DelegatingDecoderBufferProvider<media::AudioDecoderConfig>>
      provider_;
};

TEST_F(DelegatingDecoderBufferProviderTest, GetConfigAsyncSucceeds) {
  EXPECT_TRUE(provider_->IsValid());
  EXPECT_CALL(*provider_impl_, GetConfigAsync(testing::_));
  provider_->GetConfigAsync(
      base::BindOnce(&MockClient::OnConfig, base::Unretained(&client_)));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(provider_->IsValid());
}

TEST_F(DelegatingDecoderBufferProviderTest, GetConfigAsyncFails) {
  provider_impl_.reset();
  EXPECT_TRUE(provider_->IsValid());
  provider_->GetConfigAsync(
      base::BindOnce(&MockClient::OnConfig, base::Unretained(&client_)));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(provider_->IsValid());
}

TEST_F(DelegatingDecoderBufferProviderTest, ReadBufferAsyncSucceeds) {
  EXPECT_TRUE(provider_->IsValid());
  EXPECT_CALL(*provider_impl_, ReadBufferAsync(testing::_));
  provider_->ReadBufferAsync(
      base::BindOnce(&MockClient::OnBuffer, base::Unretained(&client_)));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(provider_->IsValid());
}

TEST_F(DelegatingDecoderBufferProviderTest, ReadBufferAsyncFails) {
  provider_impl_.reset();
  EXPECT_TRUE(provider_->IsValid());
  provider_->ReadBufferAsync(
      base::BindOnce(&MockClient::OnBuffer, base::Unretained(&client_)));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(provider_->IsValid());
}

TEST_F(DelegatingDecoderBufferProviderTest, SetInvalidationCallbackSucceeds) {
  EXPECT_TRUE(provider_->IsValid());
  EXPECT_CALL(*provider_impl_, SetInvalidationCallback(testing::_));
  provider_->SetInvalidationCallback(
      base::BindOnce(&MockClient::OnDeletion, base::Unretained(&client_)));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(provider_->IsValid());
}

TEST_F(DelegatingDecoderBufferProviderTest, SetInvalidationCallbackFails) {
  provider_impl_.reset();
  EXPECT_CALL(client_, OnDeletion());
  EXPECT_TRUE(provider_->IsValid());
  provider_->SetInvalidationCallback(
      base::BindOnce(&MockClient::OnDeletion, base::Unretained(&client_)));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(provider_->IsValid());
}

}  // namespace cast_streaming::webcodecs
