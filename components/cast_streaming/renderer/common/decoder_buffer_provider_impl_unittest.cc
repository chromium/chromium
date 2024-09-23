// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/renderer/common/decoder_buffer_provider_impl.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_data_pipe_read_write.h"
#include "media/mojo/mojom/media_types.mojom-forward.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cast_streaming {
namespace {

constexpr uint32_t kDefaultDataPipeCapacityBytes = 512;

class MockClient
    : public DecoderBufferProviderImpl<media::AudioDecoderConfig>::Client {
 public:
  MOCK_METHOD1(RequestBufferAsync,
               void(base::OnceCallback<void(media::mojom::DecoderBufferPtr)>));
  MOCK_METHOD0(OnDelete, void());
};

}  // namespace

class DecoderBufferProviderImplTest : public testing::Test {
 public:
  DecoderBufferProviderImplTest() {
    populated_buffer_ = media::DecoderBuffer::CopyFrom(kSerializedData);
  }

  ~DecoderBufferProviderImplTest() override = default;

  void OnBufferRead(scoped_refptr<media::DecoderBuffer> buffer) {
    EXPECT_TRUE(buffer->MatchesForTesting(*populated_buffer_));
  }

 protected:
  void CreateProvider() {
    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    ASSERT_EQ(mojo::CreateDataPipe(kDefaultDataPipeCapacityBytes,
                                   producer_handle, consumer_handle),
              MOJO_RESULT_OK);

    writer_ =
        std::make_unique<media::MojoDataPipeWriter>(std::move(producer_handle));
    provider_ =
        std::make_unique<DecoderBufferProviderImpl<media::AudioDecoderConfig>>(
            config_, std::move(consumer_handle), &client_,
            task_environment_.GetMainThreadTaskRunner());
  }

  void WriteBufferData() {
    writer_->Write(kSerializedData.data(), kSerializedData.size(),
                   base::BindOnce(&DecoderBufferProviderImplTest::OnWriteDone,
                                  base::Unretained(this)));
  }

  void OnWriteDone(bool succeeded) { ASSERT_TRUE(succeeded); }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<media::MojoDataPipeWriter> writer_;
  media::AudioDecoderConfig config_{};
  testing::StrictMock<MockClient> client_;

  std::unique_ptr<DecoderBufferProviderImpl<media::AudioDecoderConfig>>
      provider_;

  std::array<uint8_t, 3> kSerializedData = {42, 43, 44};
  scoped_refptr<media::DecoderBuffer> populated_buffer_;
};

TEST_F(DecoderBufferProviderImplTest, CreationTest) {
  CreateProvider();
  EXPECT_TRUE(provider_->IsValid());
}

TEST_F(DecoderBufferProviderImplTest, InvalidationCallbackTest) {
  CreateProvider();
  EXPECT_TRUE(provider_->IsValid());
  provider_->SetInvalidationCallback(
      base::BindOnce(&MockClient::OnDelete, base::Unretained(&client_)));
  EXPECT_TRUE(provider_->IsValid());
  EXPECT_CALL(client_, OnDelete());
  provider_.reset();
}

TEST_F(DecoderBufferProviderImplTest, ReadTest) {
  CreateProvider();

  EXPECT_CALL(client_, RequestBufferAsync(testing::_))
      .WillOnce(
          [this](base::OnceCallback<void(media::mojom::DecoderBufferPtr)> cb) {
            auto buffer = media::mojom::DecoderBuffer::From(*populated_buffer_);
            std::move(cb).Run(std::move(buffer));
          });
  WriteBufferData();

  provider_->ReadBufferAsync(base::BindOnce(
      &DecoderBufferProviderImplTest::OnBufferRead, base::Unretained(this)));
  task_environment_.RunUntilIdle();
}

}  // namespace cast_streaming
