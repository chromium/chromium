// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_FRAME_SINKS_BLIT_REQUEST_H_
#define COMPONENTS_VIZ_COMMON_FRAME_SINKS_BLIT_REQUEST_H_

#include <memory>
#include <string>

#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/viz_common_export.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "ui/gfx/geometry/point.h"

namespace viz {

// Structure describing a blit operation that can be appended to
// `CopyOutputRequest` if the callers want to place the results of the operation
// in textures that they own.
struct VIZ_COMMON_EXPORT BlitRequest {
  explicit BlitRequest(
      const gfx::Point& destination_region_offset,
      const std::array<gpu::MailboxHolder, CopyOutputResult::kMaxPlanes>&
          mailboxes);
  BlitRequest(const BlitRequest& other);
  BlitRequest& operator=(const BlitRequest& other);
  ~BlitRequest();

  // Offset from the origin of the image represented by the |mailboxes|.
  // The results of the blit request will be placed at that offset in those
  // images.
  gfx::Point destination_region_offset;
  // Mailboxes with planes that will be populated.
  // The textures can (but don't have to be) backed by
  // a GpuMemoryBuffer. The pixel format of the request determines
  // how many planes need to be present.
  std::array<gpu::MailboxHolder, CopyOutputResult::kMaxPlanes> mailboxes;

  std::string ToString() const;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_FRAME_SINKS_BLIT_REQUEST_H_
