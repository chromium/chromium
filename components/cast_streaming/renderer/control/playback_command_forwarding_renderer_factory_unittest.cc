// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/renderer/control/playback_command_forwarding_renderer_factory.h"

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "media/base/mock_audio_renderer_sink.h"
#include "media/base/mock_filters.h"
#include "media/base/mock_video_renderer_sink.h"
#include "media/base/overlay_info.h"
#include "media/base/video_renderer_sink.h"
#include "media/mojo/mojom/renderer.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"

using ::testing::_;
using ::testing::ByMove;
using ::testing::ByRef;
using ::testing::Eq;
using ::testing::Return;
using ::testing::StrictMock;

namespace cast_streaming {
namespace {

class MockOverlayInfoCbHandler {
 public:
  MOCK_METHOD2(Call, void(bool, media::ProvideOverlayInfoCB));
};

}  // namespace

class PlaybackCommandForwardingRendererFactoryTest : public testing::Test {
 public:
  PlaybackCommandForwardingRendererFactoryTest()
      : mock_factory_(
            std::make_unique<StrictMock<media::MockRendererFactory>>()),
        factory_(remote_.BindNewPipeAndPassReceiver()) {
    factory_.SetWrappedRendererFactory(mock_factory_.get());
  }

  ~PlaybackCommandForwardingRendererFactoryTest() override = default;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  mojo::Remote<media::mojom::Renderer> remote_;

  std::unique_ptr<media::MockRendererFactory> mock_factory_;
  PlaybackCommandForwardingRendererFactory factory_;
};

TEST_F(PlaybackCommandForwardingRendererFactoryTest,
       FactoryCreationCallsOtherFactoryCreate) {
  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner(
      new base::TestSimpleTaskRunner());
  scoped_refptr<base::SingleThreadTaskRunner> worker_task_runner(
      new base::TestSimpleTaskRunner());
  auto audio_sink =
      base::MakeRefCounted<StrictMock<media::MockAudioRendererSink>>();
  StrictMock<media::MockVideoRendererSink> video_sink;
  StrictMock<MockOverlayInfoCbHandler> cb_handler;
  media::RequestOverlayInfoCB mock_cb = base::BindRepeating(
      &MockOverlayInfoCbHandler::Call, base::Unretained(&cb_handler));
  gfx::ColorSpace color_space;

  EXPECT_CALL(*mock_factory_,
              CreateRenderer(Eq(ByRef(media_task_runner)),
                             Eq(ByRef(worker_task_runner)), audio_sink.get(),
                             &video_sink, testing::_, Eq(ByRef(color_space))))
      .WillOnce(Return(ByMove(std::make_unique<media::MockRenderer>())));
  factory_.CreateRenderer(media_task_runner, worker_task_runner,
                          audio_sink.get(), &video_sink, std::move(mock_cb),
                          color_space);
}

}  // namespace cast_streaming
