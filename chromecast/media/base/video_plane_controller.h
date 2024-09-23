// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_BASE_VIDEO_PLANE_CONTROLLER_H_
#define CHROMECAST_MEDIA_BASE_VIDEO_PLANE_CONTROLLER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "chromecast/public/graphics_types.h"
#include "chromecast/public/video_plane.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/overlay_transform.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace gfx {
class Rect;
}  // namespace gfx

namespace media {
struct VideoTransformation;
}  // namespace media

namespace chromecast {
namespace media {

// Provides main interface for setting video plane geometry.  All callsites
// should use this over VideoPlane::SetGeometry.  Reasons for this:
// * provides conversion between graphics plane coordinates and screen
//   resolution coordinates
// * updates VideoPlane when screen resolution changes
// * handles threading correctly (posting SetGeometry to media thread).
// * coalesces multiple calls in short space of time to prevent flooding the
//   media thread with SetGeometry calls (which are expensive on many
//   platforms).
// All public methods should be called from the same thread that the class was
// constructed on.
class VideoPlaneController {
 public:
  VideoPlaneController(
      const Size& graphics_resolution,
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner);

  VideoPlaneController(const VideoPlaneController&) = delete;
  VideoPlaneController& operator=(const VideoPlaneController&) = delete;

  ~VideoPlaneController();

  // Sets the video plane geometry in *graphics plane coordinates*. If there is
  // no change to video plane parameters from the last call to this method, it
  // is a no-op.
  void SetGeometry(const gfx::RectF& display_rect,
                   gfx::OverlayTransform transform);
  void SetGeometryFromMediaType(const gfx::Rect& display_rect,
                                const ::media::VideoTransformation& transform);

  // Sets physical screen resolution. This must be called at least once when
  // the final output resolution (HDMI signal or panel resolution) is known,
  // then later when it changes. If there is no change to the screen resolution
  // from the last call to this method, it is a no-op.
  void SetScreenResolution(const Size& resolution);

  // After Pause is called, no further calls to VideoPlane::SetGeometry will be
  // made except for any pending calls already scheduled on the media thread.
  // The Set methods will however update cached parameters that will take
  // effect once the class is resumed. Safe to call multiple times.
  // TODO(esum): Handle the case where there are pending calls already on the
  // media thread. When this returns, the caller needs to know that absolutely
  // no more SetGeometry calls will be made.
  void Pause();
  // Makes class active again, and clears any cached video plane geometry
  // parameters. Safe to call multiple times.
  void Resume();
  bool is_paused() const;

 private:
  class RateLimitedSetVideoPlaneGeometry;
  friend struct base::DefaultSingletonTraits<VideoPlaneController>;

  void SetGeometryInternal(const gfx::RectF& display_rect,
                           VideoPlane::Transform transform);

  // Check if HaveDataForSetGeometry. If not, this method is a no-op. Otherwise
  // it scales the display rect from graphics to device resolution coordinates.
  // Then posts task to media thread for VideoPlane::SetGeometry.
  void MaybeRunSetGeometry();
  // Checks if all data has been collected to make calls to
  // VideoPlane::SetGeometry.
  bool HaveDataForSetGeometry() const;
  // Clears any cached video plane geometry parameters.
  void ClearVideoPlaneGeometry();

  bool is_paused_;

  // Current resolutions
  bool have_screen_res_;
  Size screen_res_;
  const Size graphics_plane_res_;

  // Which coordinates to use for transforms (e.g. graphics coordinates or
  // screen coordinates).
  const VideoPlane::Coordinates coordinates_;

  // Saved video plane parameters (in graphics plane coordinates)
  // for use when screen resolution changes.
  bool have_video_plane_geometry_;
  RectF video_plane_display_rect_;
  VideoPlane::Transform video_plane_transform_;

  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;
  scoped_refptr<RateLimitedSetVideoPlaneGeometry> video_plane_wrapper_;

  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<VideoPlaneController> weak_factory_{this};
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_BASE_VIDEO_PLANE_CONTROLLER_H_
