// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/renderer/frame/frame_injecting_demuxer.h"

#include <optional>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/cast_streaming/renderer/frame/demuxer_connector.h"
#include "media/base/demuxer.h"
#include "media/base/mock_demuxer_host.h"
#include "media/base/pipeline_status.h"
#include "media/base/test_helpers.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
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

}  // namespace cast_streaming
