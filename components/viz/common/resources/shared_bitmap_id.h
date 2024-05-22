// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_RESOURCES_SHARED_BITMAP_ID_H_
#define COMPONENTS_VIZ_COMMON_RESOURCES_SHARED_BITMAP_ID_H_

#include <stdint.h>

#include <array>

#include "components/viz/common/viz_common_export.h"

namespace viz {

// A SharedBitmapId is an unguessable name that references a shared memory
// buffer.
// This name can be passed across processes permitting one context to share
// memory with another. The name consists of a random set of bytes.
struct VIZ_COMMON_EXPORT SharedBitmapId {
  SharedBitmapId() = default;

  static SharedBitmapId Generate();

  bool IsZero() const;

  std::strong_ordering operator<=>(const SharedBitmapId& other) const = default;

  // NOTE: The specific value used here is chosen to keep compatibility with
  // the time when SharedBitmapId was an alias for gpu::Mailbox. There is no
  // particular need for it to stay 16 at this point, but there is no particular
  // reason to change it either.
  std::array<uint8_t, 16> name = {};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_RESOURCES_SHARED_BITMAP_ID_H_
