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

  virtual ~VideoPlane() {}

  // Updates the video plane geometry.
  // |screen_rect| specifies the rectangle that the video should occupy,
  // in screen resolution coordinates.
  // |transform| specifies how the video should be transformed within that
  // rectangle.
  virtual void SetGeometry(const RectF& screen_rect, Transform transform) = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_VIDEO_PLANE_H_
