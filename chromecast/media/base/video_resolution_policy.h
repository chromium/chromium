// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_BASE_VIDEO_RESOLUTION_POLICY_H_
#define CHROMECAST_MEDIA_BASE_VIDEO_RESOLUTION_POLICY_H_

namespace gfx {
class Size;
}  // namespace gfx

namespace chromecast {
namespace media {

// Interface allowing renderer to check for whether certain video resolutions
// should have playback blocked.
// TODO(halliwell): remove this mechanism once we have PR3.
class VideoResolutionPolicy {
 public:
  // Observer allows policy subclass to notify renderer when some
  // conditions have changed.  Renderer should re-check current
  // resolution.
  class Observer {
   public:
    virtual void OnVideoResolutionPolicyChanged() = 0;

   protected:
    virtual ~Observer() {}
  };

  virtual ~VideoResolutionPolicy();

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  virtual bool ShouldBlock(const gfx::Size& size) = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_BASE_VIDEO_RESOLUTION_POLICY_H_
