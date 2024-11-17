// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_request_id.h"

#include <inttypes.h>
#include <stdint.h>

#include "base/strings/stringprintf.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"

namespace permissions {

PermissionRequestID::PermissionRequestID(
    content::RenderFrameHost* render_frame_host,
    RequestLocalId request_local_id)
    : global_render_frame_host_id_(render_frame_host->GetGlobalId()),
      request_local_id_(request_local_id) {}

PermissionRequestID::PermissionRequestID(content::GlobalRenderFrameHostId id,
                                         RequestLocalId request_local_id)
    : global_render_frame_host_id_(id), request_local_id_(request_local_id) {}

PermissionRequestID::~PermissionRequestID() = default;

PermissionRequestID::PermissionRequestID(const PermissionRequestID&) = default;
PermissionRequestID& PermissionRequestID::operator=(
    const PermissionRequestID&) = default;

bool PermissionRequestID::operator==(const PermissionRequestID& other) const {
  return global_render_frame_host_id_ == other.global_render_frame_host_id_ &&
         request_local_id_ == other.request_local_id_;
}

bool PermissionRequestID::operator!=(const PermissionRequestID& other) const {
  return !operator==(other);
}

std::string PermissionRequestID::ToString() const {
  return base::StringPrintf(
      "%d,%d,%" PRId64, global_render_frame_host_id_.child_id,
      global_render_frame_host_id_.frame_routing_id, request_local_id_.value());
}

}  // namespace permissions
