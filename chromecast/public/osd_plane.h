// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_OSD_PLANE_H_
#define CHROMECAST_PUBLIC_OSD_PLANE_H_

namespace chromecast {

class OsdSurface;
struct Rect;
struct Size;

// Abstract graphics plane for OSD, to be implemented in platform-specific way.
// Platform must composite this plane on top of main (GL-based) graphics plane.
class OsdPlane {
 public:
  virtual ~OsdPlane() {}

  // Creates a surface for offscreen drawing.
  virtual OsdSurface* CreateSurface(const Size& size) = 0;

  // Sets a clip rectangle, client should call before drawing to back buffer
  // to specify the area they intend to draw on.  Platforms may reduce memory
  // usage by only allocating back buffer to cover this area.  Areas outside
  // clip rectangle must display as fully transparent.  In particular, setting
  // an empty clip rectangle and calling Flip should clear the plane.
  // |osd_res| gives the resolution of the full OSD plane, i.e. |rect| is
  // a subrectangle of this area.  |output_scale| specifies the current scaling
  // from |osd_res| to output screen resolution.
  virtual void SetClipRectangle(const Rect& rect,
                                const Size& osd_res,
                                float output_scale) = 0;

  // Gets the current back buffer surface.  Valid until next call to Flip or
  // SetClipRectangle.
  virtual OsdSurface* GetBackBuffer() = 0;

  // Presents current back buffer to screen.
  virtual void Flip() = 0;
};

}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_OSD_PLANE_H_
