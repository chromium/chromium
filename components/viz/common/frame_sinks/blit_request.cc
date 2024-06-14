// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/frame_sinks/blit_request.h"

#include <utility>

#include "base/strings/stringprintf.h"

namespace viz {

BlendBitmap::BlendBitmap(const gfx::Rect& source_region,
                         const gfx::Rect& destination_region,
                         sk_sp<SkImage> image)
    : source_region_(source_region),
      destination_region_(destination_region),
      image_(std::move(image)) {}

BlendBitmap::BlendBitmap(BlendBitmap&& other) = default;
BlendBitmap& BlendBitmap::operator=(BlendBitmap&& other) = default;

BlendBitmap::~BlendBitmap() = default;

std::string BlendBitmap::ToString() const {
  return base::StringPrintf("blend %s from %ix%i over %s",
                            source_region_.ToString().c_str(), image_->width(),
                            image_->height(),
                            destination_region_.ToString().c_str());
}

BlitRequest::BlitRequest(const gfx::Point& destination_region_offset,
                         LetterboxingBehavior letterboxing_behavior,
                         const gpu::Mailbox& mailbox,
                         const gpu::SyncToken& sync_token,
                         bool populates_gpu_memory_buffer)
    : destination_region_offset_(destination_region_offset),
      letterboxing_behavior_(letterboxing_behavior),
      mailbox_(mailbox),
      sync_token_(sync_token),
      populates_gpu_memory_buffer_(populates_gpu_memory_buffer) {}

BlitRequest::BlitRequest(BlitRequest&& other) = default;
BlitRequest& BlitRequest::operator=(BlitRequest&& other) = default;

BlitRequest::~BlitRequest() = default;

std::string BlitRequest::ToString() const {
  return base::StringPrintf("blit to %s, blend %u bitmaps, populates GMB? %d",
                            destination_region_offset_.ToString().c_str(),
                            static_cast<uint32_t>(blend_bitmaps_.size()),
                            populates_gpu_memory_buffer_);
}

}  // namespace viz
