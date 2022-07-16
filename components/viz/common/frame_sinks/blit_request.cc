// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/frame_sinks/blit_request.h"

#include "base/strings/stringprintf.h"

namespace viz {

BlitRequest::BlitRequest(
    const gfx::Point& destination_region_offset,
    const std::array<gpu::MailboxHolder, CopyOutputResult::kMaxPlanes>&
        mailboxes)
    : destination_region_offset(destination_region_offset),
      mailboxes(mailboxes) {}

BlitRequest::BlitRequest(const BlitRequest& other) = default;
BlitRequest& BlitRequest::operator=(const BlitRequest& other) = default;

BlitRequest::~BlitRequest() = default;

std::string BlitRequest::ToString() const {
  return base::StringPrintf("blit to %s",
                            destination_region_offset.ToString().c_str());
}

}  // namespace viz
