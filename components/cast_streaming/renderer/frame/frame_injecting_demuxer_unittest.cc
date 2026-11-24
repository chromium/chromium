// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/renderer/frame/frame_injecting_demuxer.h"

#include <deque>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/tracing/test_trace_processor.h"
#include "base/test/tracing/trace_test_utils.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/cast_streaming/common/frame/frame_trace_flow.h"
#include "components/cast_streaming/renderer/frame/demuxer_connector.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer.h"
#include "media/base/mock_demuxer_host.h"
#include "media/base/pipeline_status.h"
#include "media/base/test_helpers.h"
#include "media/mojo/common/media_type_converters.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cast_streaming {

class FrameInjectingDemuxerTest : public testing::Test {
 public:
  FrameInjectingDemuxerTest()
      : media_task_runner_(base::ThreadPool::CreateSequencedTaskRunner({})) {}

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;
  DemuxerConnector demuxer_connector_;
};

TEST_F(FrameInjectingDemuxerTest, MetadataProperties) {
  std::unique_ptr<media::Demuxer> demuxer =
      std::make_unique<FrameInjectingDemuxer>(
          demuxer_connector_.config_buffer(), media_task_runner_);

  EXPECT_EQ(demuxer->GetDemuxerType(),
            media::DemuxerType::kFrameInjectingDemuxer);
  EXPECT_EQ(demuxer->GetDisplayName(), "FrameInjectingDemuxer");
  EXPECT_FALSE(demuxer->IsSeekable());
  EXPECT_EQ(demuxer->GetStartTime(), base::TimeDelta());
  EXPECT_EQ(demuxer->GetTimelineOffset(), base::Time());
  EXPECT_EQ(demuxer->GetMemoryUsage(), 0);
  EXPECT_FALSE(demuxer->GetContainerForMetrics().has_value());

  base::RunLoop run_loop;
  media_task_runner_->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                                 EXPECT_TRUE(demuxer->GetAllStreams().empty());
                                 run_loop.Quit();
                               }));
  run_loop.Run();
}

TEST_F(FrameInjectingDemuxerTest, InitializationFailedWhenNoStreams) {
  demuxer_connector_.config_buffer()->SetConfigs(nullptr, nullptr);

  auto frame_demuxer = std::make_unique<FrameInjectingDemuxer>(
      demuxer_connector_.config_buffer(), media_task_runner_);
  media::Demuxer* demuxer = frame_demuxer.get();

  testing::NiceMock<media::MockDemuxerHost> host;
  std::optional<media::PipelineStatus> status_result;

  base::RunLoop run_loop;
  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &media::Demuxer::Initialize, base::Unretained(demuxer), &host,
          base::BindLambdaForTesting([&](media::PipelineStatus status) {
            status_result = status;
            run_loop.Quit();
          })));

  run_loop.Run();

  EXPECT_TRUE(status_result.has_value());
  EXPECT_EQ(status_result.value(), media::DEMUXER_ERROR_COULD_NOT_OPEN);

  base::RunLoop delete_run_loop;
  media_task_runner_->DeleteSoon(FROM_HERE, std::move(frame_demuxer));
  media_task_runner_->PostTask(FROM_HERE, delete_run_loop.QuitClosure());
  delete_run_loop.Run();
}

TEST_F(FrameInjectingDemuxerTest, SeekAndAbortReads) {
  std::unique_ptr<media::Demuxer> demuxer =
      std::make_unique<FrameInjectingDemuxer>(
          demuxer_connector_.config_buffer(), media_task_runner_);

  testing::NiceMock<media::MockDemuxerHost> host;
  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&media::Demuxer::Initialize,
                                base::Unretained(demuxer.get()), &host,
                                base::DoNothing()));

  std::optional<media::PipelineStatus> seek_status;
  base::RunLoop run_loop;
  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &media::Demuxer::Seek, base::Unretained(demuxer.get()),
          base::Seconds(1),
          base::BindLambdaForTesting([&](media::PipelineStatus status) {
            seek_status = status;
            run_loop.Quit();
          })));

  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&media::Demuxer::AbortPendingReads,
                                base::Unretained(demuxer.get())));

  run_loop.Run();

  EXPECT_TRUE(seek_status.has_value());
  EXPECT_EQ(seek_status.value(), media::PIPELINE_OK);

  base::RunLoop delete_run_loop;
  media_task_runner_->DeleteSoon(FROM_HERE, std::move(demuxer));
  media_task_runner_->PostTask(FROM_HERE, delete_run_loop.QuitClosure());
  delete_run_loop.Run();
}

TEST_F(FrameInjectingDemuxerTest, StopInvalidatesWeakPtrs) {
  std::unique_ptr<media::Demuxer> demuxer =
      std::make_unique<FrameInjectingDemuxer>(
          demuxer_connector_.config_buffer(), media_task_runner_);

  testing::NiceMock<media::MockDemuxerHost> host;
  bool initialized_called = false;

  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &media::Demuxer::Initialize, base::Unretained(demuxer.get()), &host,
          base::BindLambdaForTesting([&](media::PipelineStatus status) {
            initialized_called = true;
          })));

  base::RunLoop run_loop;
  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&media::Demuxer::Stop, base::Unretained(demuxer.get())));
  media_task_runner_->PostTask(FROM_HERE, run_loop.QuitClosure());

  run_loop.Run();

  // Destroying demuxer on media_task_runner_ should be safe after Stop().
  base::RunLoop delete_run_loop;
  media_task_runner_->DeleteSoon(FROM_HERE, std::move(demuxer));
  media_task_runner_->PostTask(FROM_HERE, delete_run_loop.QuitClosure());
  delete_run_loop.Run();
}

TEST_F(FrameInjectingDemuxerTest, EnableBitstreamConverterAfterMojoDisconnect) {
  mojo::PendingRemote<mojom::VideoBufferRequester> video_remote;
  mojo::PendingReceiver<mojom::VideoBufferRequester> video_receiver =
      video_remote.InitWithNewPipeAndPassReceiver();

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer, consumer), MOJO_RESULT_OK);

  auto video_stream_info = mojom::VideoStreamInfo::New(
      media::TestVideoConfig::Normal(), std::move(consumer));
  auto video_init_info = mojom::VideoStreamInitializationInfo::New(
      std::move(video_remote), std::move(video_stream_info));

  demuxer_connector_.config_buffer()->SetConfigs(nullptr,
                                                 std::move(video_init_info));

  auto frame_demuxer = std::make_unique<FrameInjectingDemuxer>(
      demuxer_connector_.config_buffer(), media_task_runner_);
  media::Demuxer* demuxer = frame_demuxer.get();

  testing::NiceMock<media::MockDemuxerHost> host;
  std::optional<media::PipelineStatus> init_status;
  base::RunLoop run_loop;
  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &media::Demuxer::Initialize, base::Unretained(demuxer), &host,
          base::BindLambdaForTesting([&](media::PipelineStatus status) {
            init_status = status;
            run_loop.Quit();
          })));
  run_loop.Run();

  EXPECT_EQ(init_status, media::PIPELINE_OK);

  media::DemuxerStream* stream = nullptr;
  base::RunLoop get_stream_loop;
  media_task_runner_->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                                 auto streams = demuxer->GetAllStreams();
                                 ASSERT_EQ(streams.size(), 1u);
                                 stream = streams[0];
                                 get_stream_loop.Quit();
                               }));
  get_stream_loop.Run();
  ASSERT_TRUE(stream);

  // Close the browser side of the VideoBufferRequester pipe. A Read() is
  // aborted only once the stream has observed the disconnect.
  video_receiver.reset();
  std::optional<media::DemuxerStream::Status> read_status;
  base::RunLoop read_loop;
  media_task_runner_->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        stream->Read(1, base::BindLambdaForTesting(
                            [&](media::DemuxerStream::Status status,
                                media::DemuxerStream::DecoderBufferVector) {
                              read_status = status;
                              read_loop.Quit();
                            }));
      }));
  read_loop.Run();
  ASSERT_EQ(read_status, media::DemuxerStream::kAborted);

  base::RunLoop enable_loop;
  media_task_runner_->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                                 stream->EnableBitstreamConverter();
                                 enable_loop.Quit();
                               }));
  enable_loop.Run();

  base::RunLoop delete_run_loop;
  media_task_runner_->DeleteSoon(FROM_HERE, std::move(frame_demuxer));
  media_task_runner_->PostTask(FROM_HERE, delete_run_loop.QuitClosure());
  delete_run_loop.Run();
}

namespace {

class FakeAudioBufferRequester : public mojom::AudioBufferRequester {
 public:
  explicit FakeAudioBufferRequester(
      mojo::PendingReceiver<mojom::AudioBufferRequester> receiver,
      mojo::ScopedDataPipeProducerHandle producer)
      : receiver_(this, std::move(receiver)), producer_(std::move(producer)) {}

  void EnqueueBuffer(scoped_refptr<media::DecoderBuffer> buffer) {
    TRACE_EVENT_INSTANT(
        "cast.stream", "CastStreaming.Frame.SentToRenderer",
        base::TimeTicks::Now(),
        perfetto::Flow::Global(GetRendererHandoffFlowId(
            media::DemuxerStream::Type::AUDIO, buffer->timestamp())));
    size_t bytes_written = 0;
    ASSERT_EQ(producer_->WriteData(*buffer, MOJO_WRITE_DATA_FLAG_ALL_OR_NONE,
                                   bytes_written),
              MOJO_RESULT_OK);
    ASSERT_EQ(bytes_written, buffer->size());
    queued_buffers_.push_back(media::mojom::DecoderBuffer::From(*buffer));
    FulfillIfReady();
  }

  // mojom::AudioBufferRequester implementation.
  void GetBuffer(GetBufferCallback callback) override {
    pending_callback_ = std::move(callback);
    FulfillIfReady();
  }

  void EnableBitstreamConverter(
      EnableBitstreamConverterCallback callback) override {
    std::move(callback).Run(true);
  }

 private:
  void FulfillIfReady() {
    if (!pending_callback_ || queued_buffers_.empty()) {
      return;
    }
    auto next = std::move(queued_buffers_.front());
    queued_buffers_.pop_front();
    std::move(pending_callback_)
        .Run(mojom::GetAudioBufferResponse::NewBuffer(std::move(next)));
  }

  mojo::Receiver<mojom::AudioBufferRequester> receiver_;
  mojo::ScopedDataPipeProducerHandle producer_;
  std::deque<media::mojom::DecoderBufferPtr> queued_buffers_;
  GetBufferCallback pending_callback_;
};

}  // namespace

TEST_F(FrameInjectingDemuxerTest,
       TracesDemuxedAudioBeforeTimestampOffsetShift) {
  using Row = std::vector<std::string>;
  base::test::TracingEnvironment tracing_environment;
  base::test::TestTraceProcessor trace_processor;
  trace_processor.StartTrace("cast.stream");

  mojo::PendingRemote<mojom::AudioBufferRequester> audio_remote;
  mojo::PendingReceiver<mojom::AudioBufferRequester> audio_receiver =
      audio_remote.InitWithNewPipeAndPassReceiver();

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer, consumer), MOJO_RESULT_OK);

  FakeAudioBufferRequester requester(std::move(audio_receiver),
                                     std::move(producer));

  auto audio_stream_info = mojom::AudioStreamInfo::New(
      media::TestAudioConfig::Normal(), std::move(consumer));
  auto audio_init_info = mojom::AudioStreamInitializationInfo::New(
      std::move(audio_remote), std::move(audio_stream_info));
  demuxer_connector_.config_buffer()->SetConfigs(std::move(audio_init_info),
                                                 nullptr);

  auto frame_demuxer = std::make_unique<FrameInjectingDemuxer>(
      demuxer_connector_.config_buffer(), media_task_runner_);
  media::Demuxer* demuxer = frame_demuxer.get();

  // Enqueue two audio buffers separated by a >100ms gap so that
  // StreamTimestampOffsetTracker shifts the second buffer's timestamp in place
  // from 500ms down to 40ms.
  auto first_packet = base::MakeRefCounted<media::DecoderBuffer>(4);
  first_packet->set_timestamp(base::Milliseconds(20));
  first_packet->set_duration(base::Milliseconds(20));
  requester.EnqueueBuffer(first_packet);

  auto second_packet = base::MakeRefCounted<media::DecoderBuffer>(4);
  second_packet->set_timestamp(base::Milliseconds(500));
  second_packet->set_duration(base::Milliseconds(20));
  requester.EnqueueBuffer(second_packet);

  testing::NiceMock<media::MockDemuxerHost> host;
  base::RunLoop init_loop;
  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&media::Demuxer::Initialize, base::Unretained(demuxer),
                     &host,
                     base::BindLambdaForTesting([&](media::PipelineStatus s) {
                       EXPECT_EQ(s, media::PIPELINE_OK);
                       init_loop.Quit();
                     })));
  init_loop.Run();

  media::DemuxerStream* stream = nullptr;
  base::RunLoop stream_loop;
  media_task_runner_->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                                 auto streams = demuxer->GetAllStreams();
                                 ASSERT_EQ(streams.size(), 1u);
                                 stream = streams[0];
                                 stream_loop.Quit();
                               }));
  stream_loop.Run();
  ASSERT_TRUE(stream);

  auto read_one = [&]() {
    scoped_refptr<media::DecoderBuffer> out;
    base::RunLoop read_loop;
    media_task_runner_->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          stream->Read(1, base::BindLambdaForTesting(
                              [&](media::DemuxerStream::Status status,
                                  media::DemuxerStream::DecoderBufferVector v) {
                                EXPECT_EQ(status, media::DemuxerStream::kOk);
                                ASSERT_EQ(v.size(), 1u);
                                out = std::move(v[0]);
                                read_loop.Quit();
                              }));
        }));
    read_loop.Run();
    return out;
  };

  scoped_refptr<media::DecoderBuffer> read_first = read_one();
  scoped_refptr<media::DecoderBuffer> read_second = read_one();
  ASSERT_TRUE(read_first);
  ASSERT_TRUE(read_second);
  EXPECT_EQ(read_first->timestamp(), base::Milliseconds(20));
  EXPECT_EQ(read_second->timestamp(), base::Milliseconds(40));

  const absl::Status status = trace_processor.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();
  auto flow_result = trace_processor.RunQuery(R"(
    SELECT
      slice_out.name AS from_name,
      slice_in.name AS to_name,
      EXTRACT_ARG(slice_in.arg_set_id, 'debug.stream_type') AS stream_type,
      EXTRACT_ARG(slice_in.arg_set_id, 'debug.timestamp_us') AS timestamp_us
    FROM flow
    JOIN slice slice_out ON flow.slice_out = slice_out.id
    JOIN slice slice_in ON flow.slice_in = slice_in.id
    ORDER BY slice_in.id
  )");
  ASSERT_TRUE(flow_result.has_value()) << flow_result.error();
  EXPECT_THAT(*flow_result,
              testing::ElementsAre(
                  Row{"from_name", "to_name", "stream_type", "timestamp_us"},
                  Row{"CastStreaming.Frame.SentToRenderer",
                      "CastStreaming.Frame.Demuxed", "audio", "20000"},
                  Row{"CastStreaming.Frame.SentToRenderer",
                      "CastStreaming.Frame.Demuxed", "audio", "500000"}));

  base::RunLoop delete_run_loop;
  media_task_runner_->DeleteSoon(FROM_HERE, std::move(frame_demuxer));
  media_task_runner_->PostTask(FROM_HERE, delete_run_loop.QuitClosure());
  delete_run_loop.Run();
}

}  // namespace cast_streaming
