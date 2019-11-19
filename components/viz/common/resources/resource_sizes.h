// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_SIZES_H_
#define COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_SIZES_H_

#include <stddef.h>

#include <limits>

#include "base/macros.h"
#include "base/numerics/safe_math.h"
#include "cc/base/math_util.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/viz_resource_format_export.h"
#include "ui/gfx/geometry/size.h"

namespace viz {

class VIZ_RESOURCE_FORMAT_EXPORT ResourceSizes {
 public:
  // Returns true if the width is valid and fits in bytes, false otherwise.
  template <typename T>
  static bool VerifyWidthInBytes(int width, ResourceFormat format);
  // Returns true if the size is valid and fits in bytes, false otherwise.
  template <typename T>
  static bool VerifySizeInBytes(const gfx::Size& size, ResourceFormat format);

  // Returns true if the width is valid and fits in bytes, false otherwise.
  // Sets the bytes result in the out parameter |bytes|.
  template <typename T>
  static bool MaybeWidthInBytes(int width, ResourceFormat format, T* bytes);
  // Returns true if the size is valid and fits in bytes, false otherwise.
  // Sets the bytes result in the out parameter |bytes|.
  template <typename T>
  static bool MaybeSizeInBytes(const gfx::Size& size,
                               ResourceFormat format,
                               T* bytes);

  // Dies with a CRASH() if the width can not be represented as a positive
  // number of bytes.
  template <typename T>
  static T CheckedWidthInBytes(int width, ResourceFormat format);
  // Dies with a CRASH() if the size can not be represented as a positive
  // number of bytes.
  template <typename T>
  static T CheckedSizeInBytes(const gfx::Size& size, ResourceFormat format);

  // Returns the width in bytes but may overflow or return 0. Only do this for
  // computing widths for sizes that have already been checked.
  template <typename T>
  static T UncheckedWidthInBytes(int width, ResourceFormat format);
  // Returns the size in bytes but may overflow or return 0. Only do this for
  // sizes that have already been checked.
  template <typename T>
  static T UncheckedSizeInBytes(const gfx::Size& size, ResourceFormat format);
  // Returns the width in bytes aligned but may overflow or return 0. Only do
  // this for computing widths for sizes that have already been checked.
  template <typename T>
  static T UncheckedWidthInBytesAligned(int width, ResourceFormat format);
  // Returns the size in bytes aligned but may overflow or return 0. Only do
  // this for sizes that have already been checked.
  template <typename T>
  static T UncheckedSizeInBytesAligned(const gfx::Size& size,
                                       ResourceFormat format);

 private:
  template <typename T>
  static inline void VerifyType();

  template <typename T>
  static bool VerifyWidthInBytesInternal(int width,
                                         ResourceFormat format,
                                         bool aligned);

  template <typename T>
  static bool VerifySizeInBytesInternal(const gfx::Size& size,
                                        ResourceFormat format,
                                        bool aligned);

  template <typename T>
  static bool MaybeWidthInBytesInternal(int width,
                                        ResourceFormat format,
                                        bool aligned,
                                        T* bytes);

  template <typename T>
  static bool MaybeSizeInBytesInternal(const gfx::Size& size,
                                       ResourceFormat format,
                                       bool aligned,
                                       T* bytes);

  template <typename T>
  static T WidthInBytesInternal(int width, ResourceFormat format, bool aligned);

  template <typename T>
  static T SizeInBytesInternal(const gfx::Size& size,
                               ResourceFormat format,
                               bool aligned);

  template <typename T>
  static bool MaybeRound(base::CheckedNumeric<T>* value, T mul);

  // Not instantiable.
  ResourceSizes() = delete;
};

template <typename T>
bool ResourceSizes::VerifyWidthInBytes(int width, ResourceFormat format) {
  VerifyType<T>();
  if (width <= 0)
    return false;
  return VerifyWidthInBytesInternal<T>(width, format, false);
}

template <typename T>
bool ResourceSizes::VerifySizeInBytes(const gfx::Size& size,
                                      ResourceFormat format) {
  VerifyType<T>();
  if (size.IsEmpty())
    return false;
  return VerifySizeInBytesInternal<T>(size, format, false);
}

template <typename T>
bool ResourceSizes::MaybeWidthInBytes(int width,
                                      ResourceFormat format,
                                      T* bytes) {
  VerifyType<T>();
  if (width <= 0)
    return false;
  return MaybeWidthInBytesInternal<T>(width, format, false, bytes);
}

template <typename T>
bool ResourceSizes::MaybeSizeInBytes(const gfx::Size& size,
                                     ResourceFormat format,
                                     T* bytes) {
  VerifyType<T>();
  if (size.IsEmpty())
    return false;
  return MaybeSizeInBytesInternal<T>(size, format, false, bytes);
}

template <typename T>
T ResourceSizes::CheckedWidthInBytes(int width, ResourceFormat format) {
  VerifyType<T>();
  CHECK_GT(width, 0);
  T bytes;
  CHECK(MaybeWidthInBytesInternal<T>(width, format, false, &bytes));
  return bytes;
}

template <typename T>
T ResourceSizes::CheckedSizeInBytes(const gfx::Size& size,
                                    ResourceFormat format) {
  VerifyType<T>();
  CHECK(!size.IsEmpty());
  T bytes;
  CHECK(MaybeSizeInBytesInternal<T>(size, format, false, &bytes));
  return bytes;
}

template <typename T>
T ResourceSizes::UncheckedWidthInBytes(int width, ResourceFormat format) {
  VerifyType<T>();
  DCHECK_GT(width, 0);
  DCHECK(VerifyWidthInBytesInternal<T>(width, format, false));
  return WidthInBytesInternal<T>(width, format, false);
}

template <typename T>
T ResourceSizes::UncheckedSizeInBytes(const gfx::Size& size,
                                      ResourceFormat format) {
  VerifyType<T>();
  DCHECK(!size.IsEmpty());
  DCHECK(VerifySizeInBytesInternal<T>(size, format, false));
  return SizeInBytesInternal<T>(size, format, false);
}

template <typename T>
T ResourceSizes::UncheckedWidthInBytesAligned(int width,
                                              ResourceFormat format) {
  VerifyType<T>();
  DCHECK_GT(width, 0);
  DCHECK(VerifyWidthInBytesInternal<T>(width, format, true));
  return WidthInBytesInternal<T>(width, format, true);
}

template <typename T>
T ResourceSizes::UncheckedSizeInBytesAligned(const gfx::Size& size,
                                             ResourceFormat format) {
  VerifyType<T>();
  CHECK(!size.IsEmpty());
  DCHECK(VerifySizeInBytesInternal<T>(size, format, true));
  return SizeInBytesInternal<T>(size, format, true);
}

template <typename T>
void ResourceSizes::VerifyType() {
  static_assert(
      std::numeric_limits<T>::is_integer && !std::is_same<T, bool>::value,
      "T must be non-bool integer type. Preferred type is size_t.");
}

template <typename T>
bool ResourceSizes::VerifyWidthInBytesInternal(int width,
                                               ResourceFormat format,
                                               bool aligned) {
  T ignored;
  return MaybeWidthInBytesInternal(width, format, aligned, &ignored);
}

template <typename T>
bool ResourceSizes::VerifySizeInBytesInternal(const gfx::Size& size,
                                              ResourceFormat format,
                                              bool aligned) {
  T ignored;
  return MaybeSizeInBytesInternal(size, format, aligned, &ignored);
}

template <typename T>
bool ResourceSizes::MaybeWidthInBytesInternal(int width,
                                              ResourceFormat format,
                                              bool aligned,
                                              T* bytes) {
  base::CheckedNumeric<T> bits_per_row = BitsPerPixel(format);
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
                                             ResourceFormat format,
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
                                      ResourceFormat format,
                                      bool aligned) {
  T bytes = BitsPerPixel(format);
  bytes *= width;
  bytes = cc::MathUtil::UncheckedRoundUp<T>(bytes, 8);
  bytes /= 8;
  if (aligned)
    bytes = cc::MathUtil::UncheckedRoundUp<T>(bytes, 4);
  return bytes;
}

template <typename T>
T ResourceSizes::SizeInBytesInternal(const gfx::Size& size,
                                     ResourceFormat format,
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
