// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/frame/demuxer_stream_data_provider.h"

#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/cast_streaming/browser/common/demuxer_stream_client.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/channel_layout.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_util.h"
#include "media/base/sample_format.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_data_pipe_read_write.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cast_streaming {

class DemuxerStreamDataProviderTest : public testing::Test {
 public:
  DemuxerStreamDataProviderTest()
      : first_config_(media::AudioCodec::kMP3,
                      media::SampleFormat::kSampleFormatS16,
                      media::ChannelLayout::CHANNEL_LAYOUT_MONO,
                      24000 /* samples_per_second */,
                      media::EmptyExtraData(),
                      media::EncryptionScheme::kUnencrypted),
        second_config_(media::AudioCodec::kOpus,
                       media::SampleFormat::kSampleFormatF32,
                       media::ChannelLayout::CHANNEL_LAYOUT_STEREO,
                       48000 /* samples_per_second */,
                       media::EmptyExtraData(),
                       media::EncryptionScheme::kUnencrypted) {
    data_provider_ = std::make_unique<AudioDemuxerStreamDataProvider>(
        remote_.BindNewPipeAndPassReceiver(),
        base::BindRepeating(
            &DemuxerStreamDataProviderTest::Callbacks::RequestBuffer,
            base::Unretained(&callbacks_)),
        base::BindOnce(
            &DemuxerStreamDataProviderTest::Callbacks::OnMojoDisconnect,
            base::Unretained(&callbacks_)),
        second_config_);

    data_provider_->SetClient(client_.weak_factory_.GetWeakPtr());

    std::vector<uint8_t> data = {1, 2, 3};
    first_buffer_ = media::DecoderBuffer::CopyFrom(data);
    first_buffer_->set_duration(base::Seconds(1));
    first_buffer_->set_timestamp(base::Seconds(2));

    data = {42, 43, 44};
    second_buffer_ = media::DecoderBuffer::CopyFrom(data);
    second_buffer_->set_duration(base::Seconds(32));
    second_buffer_->set_timestamp(base::Seconds(42));

    data = {7, 8, 9};
    third_buffer_ = media::DecoderBuffer::CopyFrom(data);
    third_buffer_->set_duration(base::Seconds(10));
    third_buffer_->set_timestamp(base::Seconds(11));

    task_environment_.RunUntilIdle();
  }

  ~DemuxerStreamDataProviderTest() override = default;

 protected:
  class Callbacks {
   public:
    MOCK_METHOD1(RequestBuffer, void(base::OnceClosure));
    MOCK_METHOD0(OnMojoDisconnect, void());
    MOCK_METHOD1(OnPreloadComplete, void(media::mojom::DecoderBufferPtr));

    MOCK_METHOD0(OnGetBufferDoneCalled, void());
    void OnGetBufferDone(std::optional<media::AudioDecoderConfig> config,
                         scoped_refptr<media::DecoderBuffer> buffer_expected,
                         mojom::GetAudioBufferResponsePtr get_buffer_response) {
      if (get_buffer_response->is_buffer()) {
        scoped_refptr<media::DecoderBuffer> media_buffer(
            get_buffer_response->get_buffer()
                .To<scoped_refptr<media::DecoderBuffer>>());
        EXPECT_TRUE(buffer_expected->MatchesMetadataForTesting(*media_buffer));
      }

      ASSERT_EQ(!!config, !!get_buffer_response->is_stream_info());
      if (config) {
        EXPECT_TRUE(config->Matches(
            get_buffer_response->get_stream_info()->decoder_config));
      }

      OnGetBufferDoneCalled();
    }
  };

  class MockDemuxerStreamClient : public DemuxerStreamClient {
   public:
    ~MockDemuxerStreamClient() override = default;

    MOCK_METHOD1(EnableBitstreamConverter, void(BitstreamConverterEnabledCB));
    MOCK_METHOD0(OnNoBuffersAvailable, void());
    MOCK_METHOD0(OnError, void());

    base::WeakPtrFactory<MockDemuxerStreamClient> weak_factory_{this};
  };

  using MojoPipePair = std::pair<mojo::ScopedDataPipeProducerHandle,
                                 mojo::ScopedDataPipeConsumerHandle>;
  MojoPipePair GetMojoPipePair() {
    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    mojo::CreateDataPipe(512 /* this constant is irrelevant for these tests */,
                         producer_handle, consumer_handle);
    return MojoPipePair(std::move(producer_handle), std::move(consumer_handle));
  }

  testing::StrictMock<Callbacks> callbacks_;
  testing::StrictMock<MockDemuxerStreamClient> client_;

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  scoped_refptr<media::DecoderBuffer> first_buffer_;
  scoped_refptr<media::DecoderBuffer> second_buffer_;
  scoped_refptr<media::DecoderBuffer> third_buffer_;

  media::AudioDecoderConfig first_config_;
  media::AudioDecoderConfig second_config_;

  std::unique_ptr<AudioDemuxerStreamDataProvider> data_provider_;

  mojo::Remote<mojom::AudioBufferRequester> remote_;
};

TEST_F(DemuxerStreamDataProviderTest, DataSentInOrderExpected) {
  // Test providing a config.
  EXPECT_CALL(callbacks_, RequestBuffer(testing::_));
  remote_->GetBuffer(
      base::BindOnce(&DemuxerStreamDataProviderTest::Callbacks::OnGetBufferDone,
                     base::Unretained(&callbacks_), first_config_,
                     scoped_refptr<media::DecoderBuffer>()));
  task_environment_.RunUntilIdle();
  EXPECT_CALL(callbacks_, OnGetBufferDoneCalled());
  MojoPipePair pipes = GetMojoPipePair();
  data_provider_->OnNewStreamInfo(first_config_, std::move(pipes.second));
  EXPECT_TRUE(first_config_.Matches(data_provider_->config()));
  task_environment_.RunUntilIdle();

  // Test providing a buffer.
  EXPECT_CALL(callbacks_, RequestBuffer(testing::_));
  remote_->GetBuffer(base::BindOnce(
      &DemuxerStreamDataProviderTest::Callbacks::OnGetBufferDone,
      base::Unretained(&callbacks_), std::nullopt, first_buffer_));
  task_environment_.RunUntilIdle();
  EXPECT_CALL(callbacks_, OnGetBufferDoneCalled());
  data_provider_->ProvideBuffer(
      media::mojom::DecoderBuffer::From(*first_buffer_));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(first_config_.Matches(data_provider_->config()));

  // Test providing a different buffer.
  EXPECT_CALL(callbacks_, RequestBuffer(testing::_));
  remote_->GetBuffer(base::BindOnce(
      &DemuxerStreamDataProviderTest::Callbacks::OnGetBufferDone,
      base::Unretained(&callbacks_), std::nullopt, second_buffer_));
  task_environment_.RunUntilIdle();
  EXPECT_CALL(callbacks_, OnGetBufferDoneCalled());
  data_provider_->ProvideBuffer(
      media::mojom::DecoderBuffer::From(*second_buffer_));
  EXPECT_TRUE(first_config_.Matches(data_provider_->config()));
  task_environment_.RunUntilIdle();

  // Test providing a different config.
  EXPECT_CALL(callbacks_, RequestBuffer(testing::_));
  remote_->GetBuffer(
      base::BindOnce(&DemuxerStreamDataProviderTest::Callbacks::OnGetBufferDone,
                     base::Unretained(&callbacks_), second_config_,
                     scoped_refptr<media::DecoderBuffer>()));
  task_environment_.RunUntilIdle();
  task_environment_.RunUntilIdle();
  EXPECT_CALL(callbacks_, OnGetBufferDoneCalled());
  pipes = GetMojoPipePair();
  EXPECT_TRUE(first_config_.Matches(data_provider_->config()));
  data_provider_->OnNewStreamInfo(second_config_, std::move(pipes.second));
  EXPECT_TRUE(second_config_.Matches(data_provider_->config()));
  task_environment_.RunUntilIdle();

  // Test providing a third buffer.
  EXPECT_CALL(callbacks_, RequestBuffer(testing::_));
  remote_->GetBuffer(base::BindOnce(
      &DemuxerStreamDataProviderTest::Callbacks::OnGetBufferDone,
      base::Unretained(&callbacks_), std::nullopt, third_buffer_));
  task_environment_.RunUntilIdle();
  EXPECT_CALL(callbacks_, OnGetBufferDoneCalled());
  data_provider_->ProvideBuffer(
      media::mojom::DecoderBuffer::From(*third_buffer_));
  task_environment_.RunUntilIdle();
}

TEST_F(DemuxerStreamDataProviderTest, NoBuffersCallback) {
  EXPECT_CALL(callbacks_, RequestBuffer(testing::_))
      .WillOnce([](base::OnceClosure no_buffers_cb) {
        std::move(no_buffers_cb).Run();
      });
  EXPECT_CALL(client_, OnNoBuffersAvailable());
  remote_->GetBuffer(base::BindOnce(
      &DemuxerStreamDataProviderTest::Callbacks::OnGetBufferDone,
      base::Unretained(&callbacks_), std::nullopt, first_buffer_));
  task_environment_.RunUntilIdle();
}

TEST_F(DemuxerStreamDataProviderTest, EnableBitstreamConverter) {
  EXPECT_CALL(client_, EnableBitstreamConverter(testing::_))
      .WillOnce(
          [](base::OnceCallback<void(bool)> cb) { std::move(cb).Run(true); });
  remote_->EnableBitstreamConverter(base::OnceCallback<void(bool)>());
  task_environment_.RunUntilIdle();
}

TEST_F(DemuxerStreamDataProviderTest, BufferPreloading) {
  EXPECT_CALL(callbacks_, RequestBuffer(testing::_));
  data_provider_->PreloadBuffer(base::BindOnce(&Callbacks::OnPreloadComplete,
                                               base::Unretained(&callbacks_)));

  EXPECT_CALL(callbacks_, OnPreloadComplete(testing::_));
  data_provider_->ProvideBuffer(
      media::mojom::DecoderBuffer::From(*first_buffer_));
}

}  // namespace cast_streaming
