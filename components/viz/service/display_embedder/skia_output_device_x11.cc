// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_x11.h"

#include <utility>

#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/vk/GrVkTypes.h"
#include "ui/base/x/x11_util.h"
#include "ui/base/x/x11_util_internal.h"
#include "ui/gfx/geometry/rect.h"

namespace viz {

SkiaOutputDeviceX11::SkiaOutputDeviceX11(
    scoped_refptr<gpu::SharedContextState> context_state,
    gfx::AcceleratedWidget widget,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback)
    : SkiaOutputDeviceOffscreen(context_state,
                                true /* flipped */,
                                true /* has_alpha */,
                                did_swap_buffer_complete_callback),
      display_(gfx::GetXDisplay()),
      widget_(widget),
      gc_(XCreateGC(display_, widget_, 0, nullptr)) {
  int result = XGetWindowAttributes(display_, widget_, &attributes_);
  LOG_IF(FATAL, !result) << "XGetWindowAttributes failed for window "
                         << widget_;
  bpp_ = gfx::BitsPerPixelForPixmapDepth(display_, attributes_.depth);
  support_rendr_ = ui::QueryRenderSupport(display_);

  // |capabilities_| should be set by SkiaOutputDeviceOffscreen.
  DCHECK(capabilities_.flipped_output_surface);
  DCHECK(capabilities_.supports_post_sub_buffer);
}

SkiaOutputDeviceX11::~SkiaOutputDeviceX11() {
  XFreeGC(display_, gc_);
}

bool SkiaOutputDeviceX11::Reshape(const gfx::Size& size,
                                  float device_scale_factor,
                                  const gfx::ColorSpace& color_space,
                                  bool has_alpha,
                                  gfx::OverlayTransform transform) {
  if (!SkiaOutputDeviceOffscreen::Reshape(size, device_scale_factor,
                                          color_space, has_alpha, transform)) {
    return false;
  }
  auto ii =
      SkImageInfo::MakeN32(size.width(), size.height(), kOpaque_SkAlphaType);
  pixels_.reserve(ii.computeMinByteSize());
  return true;
}

void SkiaOutputDeviceX11::SwapBuffers(
    BufferPresentedCallback feedback,
    std::vector<ui::LatencyInfo> latency_info) {
  return PostSubBuffer(
      gfx::Rect(0, 0, sk_surface_->width(), sk_surface_->height()),
      std::move(feedback), std::move(latency_info));
}

void SkiaOutputDeviceX11::PostSubBuffer(
    const gfx::Rect& rect,
    BufferPresentedCallback feedback,
    std::vector<ui::LatencyInfo> latency_info) {
  StartSwapBuffers(std::move(feedback));

  auto ii =
      SkImageInfo::MakeN32(rect.width(), rect.height(), kOpaque_SkAlphaType);
  DCHECK_GE(pixels_.capacity(), ii.computeMinByteSize());
  SkPixmap sk_pixmap(ii, pixels_.data(), ii.minRowBytes());
  bool result = sk_surface_->readPixels(sk_pixmap, rect.x(), rect.y());
  LOG_IF(FATAL, !result) << "Failed to read pixels from offscreen SkSurface.";

  if (bpp_ == 32 || bpp_ == 16) {
    // gfx::PutARGBImage() only supports 16 and 32 bpp.
    // TODO(penghuang): Switch to XShmPutImage.
    gfx::PutARGBImage(display_, attributes_.visual, attributes_.depth, widget_,
                      gc_, static_cast<const uint8_t*>(sk_pixmap.addr()),
                      rect.width(), rect.height(), 0 /* src_x */, 0 /* src_y */,
                      rect.x() /* dst_x */, rect.y() /* dst_y */, rect.width(),
                      rect.height());
  } else if (support_rendr_) {
    Pixmap pixmap =
        XCreatePixmap(display_, widget_, rect.width(), rect.height(), 32);
    GC gc = XCreateGC(display_, pixmap, 0, nullptr);

    XImage image = {};
    image.width = rect.width();
    image.height = rect.height();
    image.depth = 32;
    image.bits_per_pixel = 32;
    image.format = ZPixmap;
    image.byte_order = LSBFirst;
    image.bitmap_unit = 8;
    image.bitmap_bit_order = LSBFirst;
    image.bytes_per_line = sk_pixmap.rowBytes();

    image.red_mask = 0xff << SK_R32_SHIFT;
    image.green_mask = 0xff << SK_G32_SHIFT;
    image.blue_mask = 0xff << SK_B32_SHIFT;
    image.data = const_cast<char*>(static_cast<const char*>(sk_pixmap.addr()));
    XPutImage(display_, pixmap, gc, &image, 0 /* src_x */, 0 /* src_y */,
              0 /* dest_x */, 0 /* dest_y */, rect.width(), rect.height());
    XFreeGC(display_, gc);

    Picture picture = XRenderCreatePicture(
        display_, pixmap, ui::GetRenderARGB32Format(display_), 0, nullptr);
    XRenderPictFormat* pictformat =
        XRenderFindVisualFormat(display_, attributes_.visual);
    Picture dest_picture =
        XRenderCreatePicture(display_, widget_, pictformat, 0, nullptr);
    XRenderComposite(display_,
                     PictOpSrc,       // op
                     picture,         // src
                     0,               // mask
                     dest_picture,    // dest
                     0,               // src_x
                     0,               // src_y
                     0,               // mask_x
                     0,               // mask_y
                     rect.x(),        // dest_x
                     rect.y(),        // dest_y
                     rect.width(),    // width
                     rect.height());  // height
    XRenderFreePicture(display_, picture);
    XRenderFreePicture(display_, dest_picture);
    XFreePixmap(display_, pixmap);
  } else {
    NOTIMPLEMENTED();
  }
  XFlush(display_);
  FinishSwapBuffers(gfx::SwapResult::SWAP_ACK,
                    gfx::Size(sk_surface_->width(), sk_surface_->height()),
                    std::move(latency_info));
}

}  // namespace viz
