// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/resources/shared_bitmap_id.h"

#include <stddef.h>

#include "base/rand_util.h"
#include "base/ranges/algorithm.h"

namespace viz {

bool SharedBitmapId::IsZero() const {
  return base::ranges::all_of(name, [](uint8_t byte) { return byte == 0; });
}

// static
SharedBitmapId SharedBitmapId::Generate() {
  SharedBitmapId result;
  // Generates cryptographically-secure bytes.
  base::RandBytes(result.name);
  return result;
}

}  // namespace viz
