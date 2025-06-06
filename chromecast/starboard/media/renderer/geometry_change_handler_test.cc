// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/media/renderer/geometry_change_handler.h"

#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "chromecast/media/service/video_geometry_setter_service.h"
#include "chromecast/starboard/media/media/mock_starboard_api_wrapper.h"
#include "mojo/core/embedder/embedder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/test/test_screen.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/overlay_transform.h"

namespace chromecast {
namespace media {
namespace {

using ::testing::InSequence;

// Runs any pending tasks that have been posted to the current sequence.
void RunPendingTasks() {
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

TEST(GeometryChangeHandlerTest, ReadsBoundsFromVideoGeometrySetterService) {
  base::test::TaskEnvironment task_environment;
  mojo::core::Init();
  display::test::TestScreen test_screen(/*create_display=*/true,
                                        /*register_screen=*/true);

  VideoGeometrySetterService geometry_setter_service;
  MockStarboardApiWrapper starboard;

  const auto plane_id = base::UnguessableToken::Create();

  const gfx::RectF geometry(0, 0, 1920, 1080);
  const gfx::OverlayTransform transform =
      gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE;
  // This will be used as an opaque ptr; its value does not matter.
  int sb_player = 7;

  EXPECT_CALL(starboard,
              SetPlayerBounds(&sb_player, 0, static_cast<int>(geometry.x()),
                              static_cast<int>(geometry.y()),
                              static_cast<int>(geometry.width()),
                              static_cast<int>(geometry.height())))
      .Times(1);

  GeometryChangeHandler handler(&geometry_setter_service, &starboard, plane_id);
  RunPendingTasks();

  static_cast<mojom::VideoGeometrySetter*>(&geometry_setter_service)
      ->SetVideoGeometry(geometry, transform, plane_id);
  RunPendingTasks();
  handler.SetSbPlayer(&sb_player);
}

TEST(GeometryChangeHandlerTest,
     ForwardsUpdatesFromVideoGeometrySetterServiceToStarboard) {
  base::test::TaskEnvironment task_environment;
  mojo::core::Init();
  display::test::TestScreen test_screen(/*create_display=*/true,
                                        /*register_screen=*/true);

  VideoGeometrySetterService geometry_setter_service;
  MockStarboardApiWrapper starboard;

  const auto plane_id = base::UnguessableToken::Create();

  const gfx::RectF geometry_1(0, 0, 1920, 1080);
  const gfx::RectF geometry_2(0, 0, 720, 1280);
  const gfx::OverlayTransform transform =
      gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE;
  // This will be used as an opaque ptr; its value does not matter.
  int sb_player = 7;

  {
    InSequence s;
    EXPECT_CALL(starboard,
                SetPlayerBounds(&sb_player, 0, static_cast<int>(geometry_1.x()),
                                static_cast<int>(geometry_1.y()),
                                static_cast<int>(geometry_1.width()),
                                static_cast<int>(geometry_1.height())))
        .Times(1);

    EXPECT_CALL(starboard,
                SetPlayerBounds(&sb_player, 0, static_cast<int>(geometry_2.x()),
                                static_cast<int>(geometry_2.y()),
                                static_cast<int>(geometry_2.width()),
                                static_cast<int>(geometry_2.height())))
        .Times(1);
  }

  GeometryChangeHandler handler(&geometry_setter_service, &starboard, plane_id);
  RunPendingTasks();

  static_cast<mojom::VideoGeometrySetter*>(&geometry_setter_service)
      ->SetVideoGeometry(geometry_1, transform, plane_id);
  RunPendingTasks();
  handler.SetSbPlayer(&sb_player);

  static_cast<mojom::VideoGeometrySetter*>(&geometry_setter_service)
      ->SetVideoGeometry(geometry_2, transform, plane_id);
  RunPendingTasks();
}

TEST(GeometryChangeHandlerTest, ReadsBoundsFromScreenResolution) {
  base::test::TaskEnvironment task_environment;
  mojo::core::Init();
  display::test::TestScreen test_screen(/*create_display=*/true,
                                        /*register_screen=*/true);

  VideoGeometrySetterService geometry_setter_service;
  MockStarboardApiWrapper starboard;

  const auto plane_id = base::UnguessableToken::Create();

  // This will be used as an opaque ptr; its value does not matter.
  int sb_player = 7;

  // Since a resolution has not been set, the bounds should be set to
  // fullscreen.
  EXPECT_CALL(
      starboard,
      SetPlayerBounds(
          &sb_player, 0,
          static_cast<int>(display::test::TestScreen::kDefaultScreenBounds.x()),
          static_cast<int>(display::test::TestScreen::kDefaultScreenBounds.y()),
          static_cast<int>(
              display::test::TestScreen::kDefaultScreenBounds.width()),
          static_cast<int>(
              display::test::TestScreen::kDefaultScreenBounds.height())))
      .Times(1);

  GeometryChangeHandler handler(&geometry_setter_service, &starboard, plane_id);
  RunPendingTasks();
  handler.SetSbPlayer(&sb_player);
  RunPendingTasks();
}

}  // namespace
}  // namespace media
}  // namespace chromecast
