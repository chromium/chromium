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

}  // namespace cast_streaming
