// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/resources/shared_bitmap_id.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "base/check.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"

namespace viz {
namespace {

SharedBitmapId GenerateSharedBitmapId() {
  SharedBitmapId result;
  // Generates cryptographically-secure bytes.
  base::RandBytes(base::as_writable_byte_span(result.name));
  return result;
}

}  // namespace

SharedBitmapId::SharedBitmapId() {
  memset(name, 0, sizeof(name));
}

bool SharedBitmapId::IsZero() const {
  for (size_t i = 0; i < std::size(name); ++i) {
    if (name[i]) {
      return false;
    }
  }
  return true;
}

SharedBitmapId SharedBitmapId::Generate() {
  return GenerateSharedBitmapId();
}

bool SharedBitmapId::operator==(const SharedBitmapId& other) const {
  return memcmp(&name, &other.name, sizeof(name)) == 0;
}

std::strong_ordering SharedBitmapId::operator<=>(
    const SharedBitmapId& other) const {
  int result = memcmp(&name, &other.name, sizeof(name));
  if (result < 0) {
    return std::strong_ordering::less;
  } else if (result > 0) {
    return std::strong_ordering::greater;
  }
  return std::strong_ordering::equal;
}

}  // namespace viz
