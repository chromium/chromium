// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_SIZES_H_
#define COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_SIZES_H_

#include <stddef.h>

#include <limits>

#include "base/check_op.h"
#include "base/component_export.h"
#include "base/numerics/safe_math.h"
#include "cc/base/math_util.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "ui/gfx/geometry/size.h"

namespace viz {

class COMPONENT_EXPORT(VIZ_SHARED_IMAGE_FORMAT) ResourceSizes {
 public:
  // Returns true if the size is valid and fits in bytes, false otherwise.
  // Sets the bytes result in the out parameter |bytes|.
  template <typename T>
  static bool MaybeSizeInBytes(const gfx::Size& size,
                               SharedImageFormat format,
                               T* bytes);

  // Dies with a CRASH() if the width can not be represented as a positive
  // number of bytes.
  // WARNING: The `format` must be single planar.
  template <typename T>
  static T CheckedWidthInBytes(int width, SharedImageFormat format);
  // Dies with a CRASH() if the size can not be represented as a positive
  // number of bytes.
  // WARNING: The `format` must be single planar.
  template <typename T>
  static T CheckedSizeInBytes(const gfx::Size& size, SharedImageFormat format);
  // Returns the width in bytes but may overflow or return 0. Only do this for
  // computing widths for sizes that have already been checked.
  // WARNING: The `format` must be single planar.
  template <typename T>
  static T UncheckedWidthInBytes(int width, SharedImageFormat format);

 private:
  template <typename T>
  static inline void VerifyType();

  template <typename T>
  static bool VerifyWidthInBytesInternal(int width,
                                         SharedImageFormat format,
                                         bool aligned);

  template <typename T>
  static bool MaybeWidthInBytesInternal(int width,
                                        SharedImageFormat format,
                                        bool aligned,
                                        T* bytes);

  template <typename T>
  static bool MaybeSizeInBytesInternal(const gfx::Size& size,
                                       SharedImageFormat format,
                                       bool aligned,
                                       T* bytes);

  template <typename T>
  static T WidthInBytesInternal(int width,
                                SharedImageFormat format,
                                bool aligned);

  template <typename T>
  static T SizeInBytesInternal(const gfx::Size& size,
                               SharedImageFormat format,
                               bool aligned);

  template <typename T>
  static bool MaybeRound(base::CheckedNumeric<T>* value, T mul);

  // Not instantiable.
  ResourceSizes() = delete;
};

template <typename T>
bool ResourceSizes::MaybeSizeInBytes(const gfx::Size& size,
                                     SharedImageFormat format,
                                     T* bytes) {
  VerifyType<T>();
  if (size.IsEmpty())
    return false;
  return MaybeSizeInBytesInternal<T>(size, format, false, bytes);
}

template <typename T>
T ResourceSizes::CheckedWidthInBytes(int width, SharedImageFormat format) {
  VerifyType<T>();
  CHECK_GT(width, 0);
  T bytes;
  CHECK(MaybeWidthInBytesInternal<T>(width, format, false, &bytes));
  return bytes;
}

template <typename T>
T ResourceSizes::CheckedSizeInBytes(const gfx::Size& size,
                                    SharedImageFormat format) {
  VerifyType<T>();
  CHECK(!size.IsEmpty());
  T bytes;
  CHECK(MaybeSizeInBytesInternal<T>(size, format, false, &bytes));
  return bytes;
}

template <typename T>
T ResourceSizes::UncheckedWidthInBytes(int width, SharedImageFormat format) {
  VerifyType<T>();
  DCHECK_GT(width, 0);
  DCHECK(VerifyWidthInBytesInternal<T>(width, format, false));
  return WidthInBytesInternal<T>(width, format, false);
}

template <typename T>
void ResourceSizes::VerifyType() {
  static_assert(
      std::numeric_limits<T>::is_integer && !std::is_same<T, bool>::value,
      "T must be non-bool integer type. Preferred type is size_t.");
}

template <typename T>
bool ResourceSizes::VerifyWidthInBytesInternal(int width,
                                               SharedImageFormat format,
                                               bool aligned) {
  T ignored;
  return MaybeWidthInBytesInternal(width, format, aligned, &ignored);
}

template <typename T>
bool ResourceSizes::MaybeWidthInBytesInternal(int width,
                                              SharedImageFormat format,
                                              bool aligned,
                                              T* bytes) {
  base::CheckedNumeric<T> bits_per_row = format.BitsPerPixel();
  bits_per_row *= width;
  if (!bits_per_row.IsValid())
    return false;

  // Roundup bits to byte (8 bits) boundary. If width is 3 and BitsPerPixel is
  // 4, then it should return 16, so that row pixels do not get truncated.
  if (!MaybeRound<T>(&bits_per_row, 8))
    return false;

  // Convert to bytes by dividing by 8. This can't fail as we've rounded to a
  // multiple of 8 above.
  base::CheckedNumeric<T> bytes_per_row = bits_per_row / 8;
  DCHECK(bytes_per_row.IsValid());

  // If aligned is true, bytes are aligned on 4-bytes boundaries for upload
  // performance, assuming that GL_PACK_ALIGNMENT or GL_UNPACK_ALIGNMENT have
  // not changed from default.
  if (aligned) {
    // This can't fail as we've just divided by 8, so we can always round up to
    // the nearest multiple of 4.
    bool succeeded = MaybeRound<T>(&bytes_per_row, 4);
    DCHECK(succeeded);
  }

  *bytes = bytes_per_row.ValueOrDie();
  return true;
}

template <typename T>
bool ResourceSizes::MaybeSizeInBytesInternal(const gfx::Size& size,
                                             SharedImageFormat format,
                                             bool aligned,
                                             T* bytes) {
  T width_in_bytes;
  if (!MaybeWidthInBytesInternal<T>(size.width(), format, aligned,
                                    &width_in_bytes)) {
    return false;
  }

  base::CheckedNumeric<T> total_bytes = width_in_bytes;
  total_bytes *= size.height();
  if (!total_bytes.IsValid())
    return false;

  *bytes = total_bytes.ValueOrDie();
  return true;
}

template <typename T>
T ResourceSizes::WidthInBytesInternal(int width,
                                      SharedImageFormat format,
                                      bool aligned) {
  T bytes = format.BitsPerPixel();
  bytes *= width;
  bytes = cc::MathUtil::UncheckedRoundUp<T>(bytes, 8);
  bytes /= 8;
  if (aligned)
    bytes = cc::MathUtil::UncheckedRoundUp<T>(bytes, 4);
  return bytes;
}

template <typename T>
T ResourceSizes::SizeInBytesInternal(const gfx::Size& size,
                                     SharedImageFormat format,
                                     bool aligned) {
  T bytes = WidthInBytesInternal<T>(size.width(), format, aligned);
  bytes *= size.height();
  return bytes;
}

template <typename T>
bool ResourceSizes::MaybeRound(base::CheckedNumeric<T>* value, T mul) {
  DCHECK(value->IsValid());
  T to_round = value->ValueOrDie();
  if (!cc::MathUtil::VerifyRoundup<T>(to_round, mul))
    return false;
  *value = cc::MathUtil::UncheckedRoundUp<T>(to_round, mul);
  return true;
}

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_SIZES_H_
