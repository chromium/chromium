// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_quota_permission_context.h"

namespace chromecast {

CastQuotaPermissionContext::CastQuotaPermissionContext() {
}

CastQuotaPermissionContext::~CastQuotaPermissionContext() {
}

void CastQuotaPermissionContext::RequestQuotaPermission(
    const content::StorageQuotaParams& params,
    int render_process_id,
    PermissionCallback callback) {
  std::move(callback).Run(
      content::QuotaPermissionContext::QUOTA_PERMISSION_RESPONSE_ALLOW);
}

}  // namespace chromecast
