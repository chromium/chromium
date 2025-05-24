// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/media/media/starboard_video_plane.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {
namespace {

using ::testing::AllOf;
using ::testing::Field;
using ::testing::FloatEq;
using ::testing::Matcher;
using ::testing::MockFunction;

constexpr VideoPlane::Transform kNoTransform =
    VideoPlane::Transform::TRANSFORM_NONE;

// Verifies that a RectF matches the fields of `rect`.
Matcher<const RectF&> MatchesRect(const RectF& rect) {
  return AllOf(Field(&RectF::x, FloatEq(rect.x)),
               Field(&RectF::y, FloatEq(rect.y)),
               Field(&RectF::width, FloatEq(rect.width)),
               Field(&RectF::height, FloatEq(rect.height)));
}

TEST(StarboardVideoPlaneTest, RegistersAndCallsCallback) {
  base::test::SingleThreadTaskEnvironment task_environment;

  const RectF resolution(123.0f, 456.0f);

  StarboardVideoPlane plane;
  MockFunction<void(const RectF& display_rect, VideoPlane::Transform transform)>
      cb;
  EXPECT_CALL(cb, Call(MatchesRect(resolution), kNoTransform)).Times(1);

  plane.RegisterCallback(base::BindLambdaForTesting(cb.AsStdFunction()));
  plane.SetGeometry(resolution, kNoTransform);
}

TEST(StarboardVideoPlaneTest, RegistersAndUnregistersCallback) {
  base::test::SingleThreadTaskEnvironment task_environment;

  const RectF resolution(123.0f, 456.0f);

  StarboardVideoPlane plane;
  MockFunction<void(const RectF& display_rect, VideoPlane::Transform transform)>
      cb;
  EXPECT_CALL(cb, Call).Times(0);

  const int64_t token =
      plane.RegisterCallback(base::BindLambdaForTesting(cb.AsStdFunction()));
  plane.UnregisterCallback(token);
  plane.SetGeometry(resolution, kNoTransform);
}

TEST(StarboardVideoPlaneTest,
     SetsGeometryIfCallbackIsRegisteredAfterGeometryWasSet) {
  base::test::SingleThreadTaskEnvironment task_environment;

  const RectF resolution(123.0f, 456.0f);
  StarboardVideoPlane plane;

  // Here we set the geometry before any callbacks are registered. The callbacks
  // should still be called with this info as soon as they're registered.
  plane.SetGeometry(resolution, kNoTransform);

  MockFunction<void(const RectF& display_rect, VideoPlane::Transform transform)>
      cb;
  EXPECT_CALL(cb, Call(MatchesRect(resolution), kNoTransform)).Times(1);

  plane.RegisterCallback(base::BindLambdaForTesting(cb.AsStdFunction()));
}

}  // namespace
}  // namespace media
}  // namespace chromecast
