// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_cancel_requests.h"

namespace enterprise_connectors {

BinaryUploadCancelRequests::BinaryUploadCancelRequests(
    enterprise_connectors::CloudOrLocalAnalysisSettings settings)
    : cloud_or_local_settings_(std::move(settings)) {}

BinaryUploadCancelRequests::~BinaryUploadCancelRequests() = default;

void BinaryUploadCancelRequests::set_user_action_id(
    const std::string& user_action_id) {
  user_action_id_ = user_action_id;
}

}  // namespace enterprise_connectors
