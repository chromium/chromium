// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/resources/shared_image_format.h"

namespace viz {

bool SharedImageFormat::IsBitmapFormatSupported() const {
  return is_single_plane() && resource_format() == RGBA_8888;
}

}  // namespace viz
