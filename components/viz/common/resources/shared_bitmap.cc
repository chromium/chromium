// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/resources/shared_bitmap.h"

#include <stddef.h>
#include <stdint.h>

#include "base/numerics/safe_math.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"

namespace viz {

SharedBitmap::SharedBitmap(uint8_t* pixels) : pixels_(pixels) {}

SharedBitmap::~SharedBitmap() {}

// static
SharedBitmapId SharedBitmap::GenerateId() {
  return SharedBitmapId::Generate();
}

}  // namespace viz
