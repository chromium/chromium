// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/player/bitmap_request.h"

namespace paint_preview {

BitmapRequest::BitmapRequest(
    const base::Optional<base::UnguessableToken>& frame_guid,
    const gfx::Rect& clip_rect,
    float scale_factor,
    BitmapRequestCallback callback)
    : frame_guid(frame_guid),
      clip_rect(clip_rect),
      scale_factor(scale_factor),
      callback(std::move(callback)) {}

BitmapRequest::~BitmapRequest() = default;

BitmapRequest& BitmapRequest::operator=(BitmapRequest&& other) = default;

BitmapRequest::BitmapRequest(BitmapRequest&& other) = default;

}  // namespace paint_preview
