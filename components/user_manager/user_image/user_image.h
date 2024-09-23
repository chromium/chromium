// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_USER_IMAGE_USER_IMAGE_H_
#define COMPONENTS_USER_MANAGER_USER_IMAGE_USER_IMAGE_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/ref_counted_memory.h"
#include "components/user_manager/user_manager_export.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace user_manager {

// Wrapper class storing a still image and its bytes representation for
// WebUI in a web-compatible format such as JPEG. Could be used for storing
// profile images and user wallpapers.
class USER_MANAGER_EXPORT UserImage {
 public:
  // The format of the user image's bytes representation. PNG can support
  // transparent background that's not supported with JPEG.
  enum ImageFormat {
    FORMAT_JPEG,
    FORMAT_PNG,
    FORMAT_UNKNOWN,
    // Useful when the image is external and animated.
    FORMAT_WEBP,
  };

  // TODO(jasontt): Explore adding a new value for image taken from camera.
  // These special values are used instead of actual default image indices.
  struct Type {
    static inline constexpr int kInvalid{-3};

    // Returned as |image_index| when user profile image is used as user image.
    static inline constexpr int kProfile{-2};

    // Returned as |image_index| when user-selected file or photo is used as
    // user image.
    static inline constexpr int kExternal{-1};
  };

  // Encodes the given bitmap to bytes representation in |image_format| for
  // WebUI. Returns nullptr on failure.
  static scoped_refptr<base::RefCountedBytes> Encode(const SkBitmap& bitmap,
                                                     ImageFormat image_format);

  // Creates a new instance from a given still frame and tries to encode it
  // to bytes representation in |image_format| for WebUI. Always returns a
  // non-null result.
  // TODO(ivankr): remove eventually.
  static std::unique_ptr<UserImage> CreateAndEncode(
      const gfx::ImageSkia& image,
      ImageFormat image_format);

  // Choose the image format suitable for the given bitmap. Returns
  // FORMAT_PNG if the bitmap contains transparent/translucent
  // pixels. Otherwise, returns FORMAT_JPEG.
  static ImageFormat ChooseImageFormat(const SkBitmap& bitmap);

  // Create instance with an empty still frame and no bytes
  // representation for WebUI.
  UserImage();

  // Creates a new instance from a given still frame without any bytes
  // representation for WebUI.
  explicit UserImage(const gfx::ImageSkia& image);

  // Creates a new instance from a given still frame and bytes
  // representation for WebUI.
  UserImage(const gfx::ImageSkia& image,
            scoped_refptr<base::RefCountedBytes> image_bytes,
            ImageFormat image_format);

  UserImage(const UserImage&) = delete;
  UserImage& operator=(const UserImage&) = delete;

  virtual ~UserImage();

  const gfx::ImageSkia& image() const { return image_; }

  // Optional bytes representation of the still image for WebUI.
  bool has_image_bytes() const { return image_bytes_ != nullptr; }
  scoped_refptr<base::RefCountedBytes> image_bytes() const {
    return image_bytes_;
  }
  // Format of the bytes representation.
  ImageFormat image_format() const { return image_format_; }

  // URL from which this image was originally downloaded, if any.
  void set_url(const GURL& url) { url_ = url; }
  GURL url() const { return url_; }

  // Whether |image_bytes| contains data in format that is considered safe to
  // decode in sensitive environment (on Login screen).
  bool is_safe_format() const { return is_safe_format_; }
  void MarkAsSafe();

  const base::FilePath& file_path() const { return file_path_; }
  void set_file_path(const base::FilePath& file_path) {
    file_path_ = file_path;
  }

 private:
  gfx::ImageSkia image_;
  scoped_refptr<base::RefCountedBytes> image_bytes_;
  GURL url_;

  // If image was loaded from the local file, file path is stored here.
  base::FilePath file_path_;
  bool is_safe_format_ = false;
  ImageFormat image_format_ = FORMAT_UNKNOWN;
};

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_USER_IMAGE_USER_IMAGE_H_
