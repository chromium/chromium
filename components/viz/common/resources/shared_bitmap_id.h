// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_RESOURCES_SHARED_BITMAP_ID_H_
#define COMPONENTS_VIZ_COMMON_RESOURCES_SHARED_BITMAP_ID_H_

#include <stdint.h>

#include <string>

#include "components/viz/common/viz_common_export.h"

// From gpu/GLES2/gl2extchromium.h
// TODO(crbug.com/337538024): Eliminate GL references from this implementation.
#ifndef GL_MAILBOX_SIZE_CHROMIUM
#define GL_MAILBOX_SIZE_CHROMIUM 16
#endif

namespace viz {

// A SharedBitmapId is an unguessable name that references a shared memory
// buffer.
// This name can be passed across processes permitting one context to share
// memory with another. The name consists of a random set of bytes.
struct VIZ_COMMON_EXPORT SharedBitmapId {
  using Name = int8_t[GL_MAILBOX_SIZE_CHROMIUM];

  SharedBitmapId();

  static SharedBitmapId Generate();

  bool IsZero() const;

  bool operator==(const SharedBitmapId& other) const;
  std::strong_ordering operator<=>(const SharedBitmapId& other) const;

  Name name;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_RESOURCES_SHARED_BITMAP_ID_H_
