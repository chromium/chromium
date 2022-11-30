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
    : render_process_id_(render_frame_host->GetProcess()->GetID()),
      render_frame_id_(render_frame_host->GetRoutingID()),
      request_local_id_(request_local_id) {}

PermissionRequestID::PermissionRequestID(int render_process_id,
                                         int render_frame_id,
                                         RequestLocalId request_local_id)
    : render_process_id_(render_process_id),
      render_frame_id_(render_frame_id),
      request_local_id_(request_local_id) {}

PermissionRequestID::~PermissionRequestID() {}

PermissionRequestID::PermissionRequestID(const PermissionRequestID&) = default;
PermissionRequestID& PermissionRequestID::operator=(
    const PermissionRequestID&) = default;

bool PermissionRequestID::operator==(const PermissionRequestID& other) const {
  return render_process_id_ == other.render_process_id_ &&
         render_frame_id_ == other.render_frame_id_ &&
         request_local_id_ == other.request_local_id_;
}

bool PermissionRequestID::operator!=(const PermissionRequestID& other) const {
  return !operator==(other);
}

std::string PermissionRequestID::ToString() const {
  return base::StringPrintf("%d,%d,%" PRId64, render_process_id_,
                            render_frame_id_, request_local_id_.value());
}

}  // namespace permissions
