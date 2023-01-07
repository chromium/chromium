// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_OSD_SURFACE_H_
#define CHROMECAST_PUBLIC_OSD_SURFACE_H_

namespace chromecast {

struct Rect;
struct Size;

// Provides simple API (copy bitmap, blit, composite and fill) for drawing
// OSD graphics on OsdPlane.  Hardware-specific implementation should be
// instantiated by OsdPlane.
class OsdSurface {
 public:
  struct Point {
    Point(int arg_x, int arg_y) : x(arg_x), y(arg_y) {}

    const int x;
    const int y;
  };

  virtual ~OsdSurface() {}

  // Blits(fast copy) bitmap from a surface. Copies |src_rect| area of surface
  // |dst_point| of this surface.
  virtual void Blit(OsdSurface* src_surface,
                    const Rect& src_rect,
                    const Point& dst_point) = 0;

  // Composites ARGB values of |src_surface| on top of this surface. It is NOT
  // copy. Used when displaying OSD images on same plane.
  virtual void Composite(OsdSurface* src_surface,
                         const Rect& src_rect,
                         const Point& dst_point) = 0;

  // Copies |damage_rect| area of src bitmap into |dst_point| of this surface.
  // It is similar to Blit() except that it accepts arbitrary bitmap pointer
  // instead of surface.
  virtual void CopyBitmap(char* src_bitmap,
                          const Rect& src_rect,
                          const Rect& damage_rect,
                          const Point& dst_point) = 0;

  // Fills |rect| area of surface with |argb| value.
  virtual void Fill(const Rect& rect, int argb) = 0;

  // Returns the dimensions of the surface.
  virtual const Size& size() const = 0;
};

}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_OSD_SURFACE_H_
