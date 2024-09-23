// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/base/video_plane_controller.h"

#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "chromecast/public/cast_media_shlib.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/overlay_transform.h"

namespace chromecast {
namespace media {

namespace {

using ::testing::AllOf;
using ::testing::Field;
using ::testing::FloatEq;
using ::testing::Matcher;
using ::testing::NotNull;

// Returns a matcher that matches a chromecast::RectF's x, y, width, and height
// values.
auto RectFEquals(float x, float y, float width, float height) {
  return AllOf(Field(&chromecast::RectF::x, FloatEq(x)),
               Field(&chromecast::RectF::y, FloatEq(y)),
               Field(&chromecast::RectF::width, FloatEq(width)),
               Field(&chromecast::RectF::height, FloatEq(height)));
}

class MockVideoPlane : public VideoPlane {
 public:
  MockVideoPlane() : VideoPlane() {}
  ~MockVideoPlane() override = default;

  MOCK_METHOD(void, SetGeometry, (const RectF&, Transform), (override));
};

MockVideoPlane* g_video_plane = nullptr;

VideoPlane::Coordinates g_video_plane_coordinates =
    VideoPlane::Coordinates::kScreen;

}  // namespace

// Static function definitions for CastMediaShlib and VideoPlane.

void CastMediaShlib::Initialize(const std::vector<std::string>& argv) {
  CHECK(g_video_plane == nullptr);
  g_video_plane = new MockVideoPlane;
}

VideoPlane* CastMediaShlib::GetVideoPlane() {
  return g_video_plane;
}

void CastMediaShlib::Finalize() {
  if (g_video_plane) {
    delete g_video_plane;
    g_video_plane = nullptr;
  }
}

// Tests can set g_video_plane_coordinates to influence how VideoPlaneController
// should report coordinates (screen resolution vs graphics resolution).
VideoPlane::Coordinates VideoPlane::GetCoordinates() {
  return g_video_plane_coordinates;
}

// End of static function definitions.

namespace {

const chromecast::Size kDefaultSize(1920, 1080);

// A test fixture is used to ensure that CastMediaShlib is
// initialized/finalized, and to control the lifetime of the
// SingleThreadTaskEnvironment.
class VideoPlaneControllerTest : public ::testing::Test {
 protected:
  VideoPlaneControllerTest() { CastMediaShlib::Initialize({}); }

  ~VideoPlaneControllerTest() override { CastMediaShlib::Finalize(); }

  base::test::SingleThreadTaskEnvironment environment_;
};

TEST_F(VideoPlaneControllerTest, SetsGeometryOnVideoPlane) {
  VideoPlaneController controller(
      kDefaultSize, base::SingleThreadTaskRunner::GetCurrentDefault());
  controller.SetScreenResolution(kDefaultSize);

  const gfx::RectF video_geometry(1280, 720);

  ASSERT_THAT(g_video_plane, NotNull());
  EXPECT_CALL(*g_video_plane,
              SetGeometry(RectFEquals(0, 0, 1280, 720),
                          VideoPlane::Transform::TRANSFORM_NONE))
      .Times(1);

  controller.SetGeometry(video_geometry,
                         gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE);

  base::RunLoop().RunUntilIdle();
}

TEST_F(VideoPlaneControllerTest,
       SetsGeometryOnVideoPlaneAndTranslatesToScreenCoordinates) {
  g_video_plane_coordinates = VideoPlane::Coordinates::kScreen;

  const chromecast::Size kScreenResolution(3840, 2160);
  const chromecast::Size kGraphicsResolution(1920, 1080);

  VideoPlaneController controller(
      kGraphicsResolution, base::SingleThreadTaskRunner::GetCurrentDefault());
  controller.SetScreenResolution(kScreenResolution);

  const gfx::RectF video_geometry(960, 0, 960, 1080);

  ASSERT_THAT(g_video_plane, NotNull());
  // Note that the expected rect is scaled to the physical screen resolution.
  EXPECT_CALL(*g_video_plane,
              SetGeometry(RectFEquals(1920, 0, 1920, 2160),
                          VideoPlane::Transform::TRANSFORM_NONE))
      .Times(1);

  controller.SetGeometry(video_geometry,
                         gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE);

  base::RunLoop().RunUntilIdle();
}

TEST_F(VideoPlaneControllerTest,
       SetsGeometryOnVideoPlaneAndDoesNotScaleForGraphicsCoordinates) {
  g_video_plane_coordinates = VideoPlane::Coordinates::kGraphics;

  const chromecast::Size kScreenResolution(3840, 2160);
  const chromecast::Size kGraphicsResolution(1920, 1080);

  VideoPlaneController controller(
      kGraphicsResolution, base::SingleThreadTaskRunner::GetCurrentDefault());
  controller.SetScreenResolution(kScreenResolution);

  const gfx::RectF video_geometry(960, 0, 960, 1080);

  ASSERT_THAT(g_video_plane, NotNull());
  // Note that the expected rect is NOT scaled to the physical screen
  // resolution, due to the value of g_video_plane_coordinates.
  EXPECT_CALL(*g_video_plane,
              SetGeometry(RectFEquals(960, 0, 960, 1080),
                          VideoPlane::Transform::TRANSFORM_NONE))
      .Times(1);

  controller.SetGeometry(video_geometry,
                         gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE);

  base::RunLoop().RunUntilIdle();
}

}  // namespace
}  // namespace media
}  // namespace chromecast
