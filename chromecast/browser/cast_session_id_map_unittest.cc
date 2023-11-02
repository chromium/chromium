// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_session_id_map.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "content/public/test/test_content_client_initializer.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace {
const char kTestSessionId[] = "test_session_id";
}  // namespace

namespace chromecast {
namespace shell {

class CastSessionIdMapTest : public content::RenderViewHostTestHarness {
 public:
  CastSessionIdMapTest() : task_runner_(new base::TestSimpleTaskRunner) {}

  CastSessionIdMapTest(const CastSessionIdMapTest&) = delete;
  CastSessionIdMapTest& operator=(const CastSessionIdMapTest&) = delete;

  void SetUp() override {
    // Required for creating TestWebContents.
    gl::GLSurfaceTestSupport::InitializeOneOff();
    initializer_ = std::make_unique<content::TestContentClientInitializer>();
    content::RenderViewHostTestHarness::SetUp();

    // Create the map.
    cast_session_id_map_ = CastSessionIdMap::GetInstance(task_runner_.get());
  }

 protected:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  std::unique_ptr<content::TestContentClientInitializer> initializer_;
  CastSessionIdMap* cast_session_id_map_;
};

TEST_F(CastSessionIdMapTest, DefaultsToEmptyString) {
  std::string saved_session_id = cast_session_id_map_->GetSessionId("");
  EXPECT_EQ(saved_session_id, "");
}

TEST_F(CastSessionIdMapTest, CanRetrieveSessionId) {
  auto web_contents = CreateTestWebContents();
  base::UnguessableToken group_id = web_contents->GetAudioGroupId();
  cast_session_id_map_->SetAppProperties(
      kTestSessionId, false /* is_audio_app */, web_contents.get());
  task_runner_->RunUntilIdle();

  std::string saved_session_id =
      cast_session_id_map_->GetSessionId(group_id.ToString());
  EXPECT_EQ(saved_session_id, kTestSessionId);
}

TEST_F(CastSessionIdMapTest, RemovesMappingOnWebContentsDestroyed) {
  auto web_contents = CreateTestWebContents();
  base::UnguessableToken group_id = web_contents->GetAudioGroupId();
  cast_session_id_map_->SetAppProperties(
      kTestSessionId, false /* is_audio_app */, web_contents.get());
  task_runner_->RunUntilIdle();

  web_contents.reset();
  task_runner_->RunUntilIdle();

  std::string saved_session_id =
      cast_session_id_map_->GetSessionId(group_id.ToString());
  EXPECT_EQ(saved_session_id, "");
}

TEST_F(CastSessionIdMapTest, CanHoldMultiple) {
  const std::string test_session_id_1 = "test_session_id_1";
  const std::string test_session_id_2 = "test_session_id_2";
  const std::string test_session_id_3 = "test_session_id_3";
  auto web_contents_1 = CreateTestWebContents();
  auto web_contents_2 = CreateTestWebContents();
  auto web_contents_3 = CreateTestWebContents();
  cast_session_id_map_->SetAppProperties(
      test_session_id_1, false /* is_audio_app */, web_contents_1.get());
  cast_session_id_map_->SetAppProperties(
      test_session_id_2, false /* is_audio_app */, web_contents_2.get());
  cast_session_id_map_->SetAppProperties(
      test_session_id_3, false /* is_audio_app */, web_contents_3.get());
  task_runner_->RunUntilIdle();

  std::string saved_session_id = "";
  base::UnguessableToken group_id = web_contents_1->GetAudioGroupId();
  if (group_id) {
    saved_session_id = cast_session_id_map_->GetSessionId(group_id.ToString());
    EXPECT_EQ(saved_session_id, test_session_id_1);
  }

  group_id = web_contents_2->GetAudioGroupId();
  if (group_id) {
    saved_session_id = cast_session_id_map_->GetSessionId(group_id.ToString());
    EXPECT_EQ(saved_session_id, test_session_id_2);
  }

  group_id = web_contents_3->GetAudioGroupId();
  if (group_id) {
    saved_session_id = cast_session_id_map_->GetSessionId(group_id.ToString());
    EXPECT_EQ(saved_session_id, test_session_id_3);
  }
}

}  // namespace shell
}  // namespace chromecast
