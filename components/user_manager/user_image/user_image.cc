// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/user_image/user_image.h"

#include <memory>

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"

namespace user_manager {

namespace {

// Default quality for encoding user images.
const int kDefaultEncodingQuality = 90;

}  // namespace

// static
scoped_refptr<base::RefCountedBytes> UserImage::Encode(
    const SkBitmap& bitmap,
    ImageFormat image_format) {
  TRACE_EVENT2("oobe", "UserImage::Encode",
               "width", bitmap.width(), "height", bitmap.height());
  std::vector<unsigned char> output;
  if (image_format == FORMAT_JPEG) {
    if (gfx::JPEGCodec::Encode(bitmap, kDefaultEncodingQuality, &output)) {
      return base::RefCountedBytes::TakeVector(&output);
    }
  } else if (image_format == FORMAT_PNG) {
    auto* bitmap_data =
        reinterpret_cast<unsigned char*>(bitmap.getAddr32(0, 0));
    if (gfx::PNGCodec::Encode(
            bitmap_data,
            gfx::PNGCodec::FORMAT_SkBitmap,
            gfx::Size(bitmap.width(), bitmap.height()),
            bitmap.width() * bitmap.bytesPerPixel(),
            false,  // discard_transparency
            std::vector<gfx::PNGCodec::Comment>(), &output)) {
      return base::RefCountedBytes::TakeVector(&output);
    }
  } else {
    LOG(FATAL) << "Invalid image format: " << image_format;
  }
  return nullptr;
}

// static
std::unique_ptr<UserImage> UserImage::CreateAndEncode(
    const gfx::ImageSkia& image,
    ImageFormat image_format) {
  if (image.isNull())
    return std::make_unique<UserImage>();

  scoped_refptr<base::RefCountedBytes> image_bytes = Encode(*image.bitmap(),
                                                            image_format);
  if (image_bytes) {
    std::unique_ptr<UserImage> result(
        new UserImage(image, image_bytes, image_format));
    result->MarkAsSafe();
    return result;
  }
  return std::make_unique<UserImage>(image);
}

// static
UserImage::ImageFormat UserImage::ChooseImageFormat(const SkBitmap& bitmap) {
  return SkBitmap::ComputeIsOpaque(bitmap) ? FORMAT_JPEG : FORMAT_PNG;
}

UserImage::UserImage() {
}

UserImage::UserImage(const gfx::ImageSkia& image)
    : image_(image) {
}

UserImage::UserImage(const gfx::ImageSkia& image,
                     scoped_refptr<base::RefCountedBytes> image_bytes,
                     ImageFormat image_format)
    : image_(image),
      image_bytes_(image_bytes),
      image_format_(image_format) {
}

UserImage::~UserImage() = default;

void UserImage::MarkAsSafe() {
  is_safe_format_ = true;
}

}  // namespace user_manager
