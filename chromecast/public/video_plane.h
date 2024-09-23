// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_VIDEO_PLANE_H_
#define CHROMECAST_PUBLIC_VIDEO_PLANE_H_

namespace chromecast {
struct RectF;

namespace media {

class VideoPlane {
 public:
  // List of possible hardware transforms that can be applied to video.
  // Rotations are anticlockwise.
  enum Transform {
    TRANSFORM_NONE,
    ROTATE_90,
    ROTATE_180,
    ROTATE_270,
    FLIP_HORIZONTAL,
    FLIP_VERTICAL,
  };

  // List of supported coordinates for VideoPlane. kScreen represents screen
  // coordinates, meaning coordinates relative to the physical screen
  // resolution. kGraphics represents coordinates relative to the graphics
  // resolution.
  enum Coordinates {
    kScreen,
    kGraphics,
  };

  virtual ~VideoPlane() {}

  // Updates the video plane geometry.
  // |screen_rect| specifies the rectangle that the video should occupy,
  // in the coordinates specified by GetVideoPlaneCoordinates. If that function
  // is not defined, defaults to using screen resolution coordinates.
  // |transform| specifies how the video should be transformed within that
  // rectangle.
  virtual void SetGeometry(const RectF& screen_rect, Transform transform) = 0;

  // Returns the coordinate system to be used when passing coordinates to
  // VideoPlane::SetGeometry. If this is not defined, defaults to screen
  // coordinates.
  static Coordinates GetCoordinates() __attribute__((weak));
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_VIDEO_PLANE_H_
