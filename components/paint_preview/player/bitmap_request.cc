// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/player/bitmap_request.h"

namespace paint_preview {

BitmapRequest::BitmapRequest(
    const std::optional<base::UnguessableToken>& frame_guid,
    const gfx::Rect& clip_rect,
    float scale_factor,
    BitmapRequestCallback callback,
    bool run_callback_on_default_task_runner)
    : frame_guid(frame_guid),
      clip_rect(clip_rect),
      scale_factor(scale_factor),
      callback(std::move(callback)),
      run_callback_on_default_task_runner(run_callback_on_default_task_runner) {
}

BitmapRequest::~BitmapRequest() = default;

BitmapRequest& BitmapRequest::operator=(BitmapRequest&& other) noexcept =
    default;

BitmapRequest::BitmapRequest(BitmapRequest&& other) noexcept = default;

}  // namespace paint_preview
