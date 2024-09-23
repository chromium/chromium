// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/renderer/common/buffer_requester.h"

#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/channel_layout.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_util.h"
#include "media/base/sample_format.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_data_pipe_read_write.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cast_streaming {
namespace {

class MockAudioBufferRequestReceiver : public mojom::AudioBufferRequester {
 public:
  MockAudioBufferRequestReceiver() : receiver_(this) {}

  MOCK_METHOD1(GetBuffer, void(GetBufferCallback));
  MOCK_METHOD1(EnableBitstreamConverter,
               void(EnableBitstreamConverterCallback));

  mojo::PendingRemote<mojom::AudioBufferRequester> GetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<mojom::AudioBufferRequester> receiver_;
};

class MockClient : public AudioBufferRequester::Client {
 public:
  MOCK_METHOD1(
      OnNewBufferProvider,
      void(base::WeakPtr<DecoderBufferProvider<media::AudioDecoderConfig>>));
  MOCK_METHOD0(OnMojoDisconnect, void());

  MOCK_METHOD1(OnBufferReceivedOverMojo, void(media::mojom::DecoderBufferPtr));
};

}  // namespace

class BufferRequesterTest : public testing::Test {
 public:
  BufferRequesterTest()
      : first_config_(media::AudioCodec::kMP3,
                      media::SampleFormat::kSampleFormatS16,
                      media::ChannelLayout::CHANNEL_LAYOUT_MONO,
                      24000 /* this constant is irrelevant for these tests */,
                      media::EmptyExtraData(),
                      media::EncryptionScheme::kUnencrypted),
        second_config_(media::AudioCodec::kOpus,
                       media::SampleFormat::kSampleFormatF32,
                       media::ChannelLayout::CHANNEL_LAYOUT_STEREO,
                       48000 /* this constant is irrelevant for these tests */,
                       media::EmptyExtraData(),
                       media::EncryptionScheme::kUnencrypted) {
    MojoPipePair mojo_pair = GetMojoPipePair();
    buffer_requester_ = std::make_unique<AudioBufferRequester>(
        &client_, first_config_, std::move(mojo_pair.second),
        mojo_receiver_.GetRemote(),
        task_environment_.GetMainThreadTaskRunner());

    std::vector<uint8_t> data = {1, 2, 3};
    buffer_ = media::DecoderBuffer::CopyFrom(data);
    buffer_->set_duration(base::Seconds(1));
    buffer_->set_timestamp(base::Seconds(2));

    EXPECT_CALL(client_, OnNewBufferProvider(testing::_))
        .WillOnce(
            [this](
                base::WeakPtr<DecoderBufferProvider<media::AudioDecoderConfig>>
                    provider) { buffer_provider_ = std::move(provider); });
    task_environment_.RunUntilIdle();
    EXPECT_TRUE(!!buffer_provider_);
  }

  ~BufferRequesterTest() override = default;

 protected:
  using GetBufferCallback = mojom::AudioBufferRequester::GetBufferCallback;
  using MojoPipePair = std::pair<mojo::ScopedDataPipeProducerHandle,
                                 mojo::ScopedDataPipeConsumerHandle>;
  MojoPipePair GetMojoPipePair() {
    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    mojo::CreateDataPipe(512 /* this constant is irrelevant for these tests */,
                         producer_handle, consumer_handle);
    return MojoPipePair(std::move(producer_handle), std::move(consumer_handle));
  }

  std::unique_ptr<AudioBufferRequester> buffer_requester_;
  testing::StrictMock<MockAudioBufferRequestReceiver> mojo_receiver_;
  testing::StrictMock<MockClient> client_;
  mojo::ScopedDataPipeProducerHandle data_producer_;
  base::WeakPtr<DecoderBufferProvider<media::AudioDecoderConfig>>
      buffer_provider_;

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  scoped_refptr<media::DecoderBuffer> buffer_;

  media::AudioDecoderConfig first_config_;
  media::AudioDecoderConfig second_config_;
};

TEST_F(BufferRequesterTest, BufferReceivedOverMojoTriggersCallback) {
  EXPECT_CALL(client_, OnBufferReceivedOverMojo(testing::_))
      .WillOnce([this](media::mojom::DecoderBufferPtr response) {
        ASSERT_TRUE(!!response);
        scoped_refptr<media::DecoderBuffer> media_buffer(
            response.To<scoped_refptr<media::DecoderBuffer>>());
        EXPECT_TRUE(media_buffer->MatchesMetadataForTesting(*buffer_));
      });
  EXPECT_CALL(mojo_receiver_, GetBuffer(testing::_))
      .WillOnce([this](GetBufferCallback cb) {
        std::move(cb).Run(mojom::GetAudioBufferResponse::NewBuffer(
            media::mojom::DecoderBuffer::From(*buffer_)));
      });
  buffer_requester_->RequestBufferAsync(base::BindOnce(
      &MockClient::OnBufferReceivedOverMojo, base::Unretained(&client_)));
  task_environment_.RunUntilIdle();
}

TEST_F(BufferRequesterTest, ConfigReceivedOverMojoCallsClientCallback) {
  EXPECT_CALL(client_, OnNewBufferProvider(testing::_))
      .WillOnce(
          [this](base::WeakPtr<DecoderBufferProvider<media::AudioDecoderConfig>>
                     new_provider) {
            ASSERT_TRUE(!!new_provider);
            EXPECT_FALSE(!!buffer_provider_);
          });
  EXPECT_CALL(mojo_receiver_, GetBuffer(testing::_))
      .WillOnce([this](GetBufferCallback cb) {
        MojoPipePair pipes = GetMojoPipePair();
        mojom::AudioStreamInfoPtr stream_info(std::in_place_t(), second_config_,
                                              std::move(pipes.second));
        data_producer_ = std::move(pipes.first);
        std::move(cb).Run(mojom::GetAudioBufferResponse::NewStreamInfo(
            std::move(stream_info)));
      });
  buffer_requester_->RequestBufferAsync(base::BindOnce(
      &MockClient::OnBufferReceivedOverMojo, base::Unretained(&client_)));
  task_environment_.RunUntilIdle();
}

}  // namespace cast_streaming
