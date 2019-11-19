// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/application_media_info_manager.h"

#include <memory>
#include <utility>

#include "base/bind.h"
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
    application_media_info_manager_ =
        std::make_unique<ApplicationMediaInfoManager>(
            main_rfh(),
            application_media_info_manager_remote_.BindNewPipeAndPassReceiver(),
            kSessionId, kMixedAudioEnabled);
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
  std::unique_ptr<ApplicationMediaInfoManager> application_media_info_manager_;
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
