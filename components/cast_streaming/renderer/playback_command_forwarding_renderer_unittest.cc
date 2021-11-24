// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/renderer/playback_command_forwarding_renderer.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/cast_streaming/public/mojom/renderer_controller.mojom.h"
#include "media/base/mock_filters.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cast_streaming {

class PlaybackCommandForwardingRendererTest : public testing::Test {
 public:
  PlaybackCommandForwardingRendererTest() {
    auto mock_renderer =
        std::make_unique<testing::StrictMock<media::MockRenderer>>();
    mock_renderer_ = mock_renderer.get();
    renderer_ = std::make_unique<PlaybackCommandForwardingRenderer>(
        std::move(mock_renderer), task_environment_.GetMainThreadTaskRunner(),
        remote_.BindNewPipeAndPassReceiver());
  }

  ~PlaybackCommandForwardingRendererTest() override = default;

  MOCK_METHOD1(OnInitializationComplete, void(media::PipelineStatus));

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  mojo::Remote<media::mojom::Renderer> remote_;

  media::MockRenderer* mock_renderer_;
  std::unique_ptr<PlaybackCommandForwardingRenderer> renderer_;
};

TEST_F(PlaybackCommandForwardingRendererTest,
       RendererInitializeInitializesMojoPipe) {
  testing::StrictMock<media::MockMediaResource> mock_media_resource;
  testing::StrictMock<media::MockRendererClient> mock_renderer_client;

  auto init_cb = base::BindOnce(
      &PlaybackCommandForwardingRendererTest::OnInitializationComplete,
      base::Unretained(this));

  EXPECT_CALL(*mock_renderer_, OnInitialize(&mock_media_resource,
                                            &mock_renderer_client, testing::_))
      .WillOnce([this](media::MediaResource* media_resource,
                       media::RendererClient* client,
                       media::PipelineStatusCallback& init_cb) {
        auto result = base::BindOnce(std::move(init_cb),
                                     media::PipelineStatus::PIPELINE_OK);
        task_environment_.GetMainThreadTaskRunner()->PostTask(
            FROM_HERE, std::move(result));
      });

  remote_->SetPlaybackRate(1.0);
  remote_->StartPlayingFrom(base::TimeDelta{});
  task_environment_.RunUntilIdle();

  renderer_->Initialize(&mock_media_resource, &mock_renderer_client,
                        std::move(init_cb));

  EXPECT_CALL(*this,
              OnInitializationComplete(media::PipelineStatus::PIPELINE_OK));
  EXPECT_CALL(*mock_renderer_, SetPlaybackRate(1.0));
  EXPECT_CALL(*mock_renderer_, StartPlayingFrom(testing::_));
  task_environment_.RunUntilIdle();
}

}  // namespace cast_streaming
