// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/application_media_info_manager.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chromecast/browser/cast_session_id_map.h"
#include "content/public/test/test_content_client_initializer.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

using ::testing::_;
using ::testing::Invoke;

namespace {

const char kSessionId[] = "test-session-id";
constexpr bool kMixedAudioEnabled = true;

}  // namespace

class ApplicationMediaInfoManagerTest
    : public content::RenderViewHostTestHarness {
 public:
  ApplicationMediaInfoManagerTest() : started_(false) {}
  ~ApplicationMediaInfoManagerTest() override {}

  void SetUp() override {
    initializer_ = std::make_unique<content::TestContentClientInitializer>();
    content::RenderViewHostTestHarness::SetUp();
    shell::CastSessionIdMap::GetInstance(
        base::SequencedTaskRunner::GetCurrentDefault().get());
    application_media_info_manager_ =
        &ApplicationMediaInfoManager::CreateForTesting(
            *main_rfh(), kSessionId, kMixedAudioEnabled,
            application_media_info_manager_remote_
                .BindNewPipeAndPassReceiver());
  }

  void OnCastApplicationMediaInfo(
      ::media::mojom::CastApplicationMediaInfoPtr ptr) {
    EXPECT_EQ(ptr->application_session_id, kSessionId);
    EXPECT_EQ(ptr->mixer_audio_enabled, kMixedAudioEnabled);
    started_ = true;
  }

  mojo::Remote<::media::mojom::CastApplicationMediaInfoManager>
      application_media_info_manager_remote_;
  std::unique_ptr<content::TestContentClientInitializer> initializer_;
  // `ApplicationMediaInfoManager` is a `DocumentService` and manages its
  // own lifetime.
  ApplicationMediaInfoManager* application_media_info_manager_;
  bool started_;
};

TEST_F(ApplicationMediaInfoManagerTest, NoBlock_GetMediaInfo) {
  application_media_info_manager_remote_->GetCastApplicationMediaInfo(
      base::BindOnce(
          &ApplicationMediaInfoManagerTest::OnCastApplicationMediaInfo,
          base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(started_);
}

TEST_F(ApplicationMediaInfoManagerTest, Block_GetMediaInfo_Unblock) {
  application_media_info_manager_->SetRendererBlock(true);
  base::RunLoop().RunUntilIdle();
  application_media_info_manager_remote_->GetCastApplicationMediaInfo(
      base::BindOnce(
          &ApplicationMediaInfoManagerTest::OnCastApplicationMediaInfo,
          base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(started_);
  application_media_info_manager_->SetRendererBlock(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(started_);
}

TEST_F(ApplicationMediaInfoManagerTest, Block_Unblock_GetMediaInfo) {
  application_media_info_manager_->SetRendererBlock(true);
  base::RunLoop().RunUntilIdle();
  application_media_info_manager_->SetRendererBlock(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(started_);
  application_media_info_manager_remote_->GetCastApplicationMediaInfo(
      base::BindOnce(
          &ApplicationMediaInfoManagerTest::OnCastApplicationMediaInfo,
          base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(started_);
}

}  // namespace media
}  // namespace chromecast
