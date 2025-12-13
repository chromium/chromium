// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_ack.h"

namespace enterprise_connectors {

BinaryUploadAck::BinaryUploadAck(
    enterprise_connectors::CloudOrLocalAnalysisSettings settings)
    : cloud_or_local_settings_(std::move(settings)) {}

BinaryUploadAck::~BinaryUploadAck() = default;

void BinaryUploadAck::set_request_token(const std::string& token) {
  ack_.set_request_token(token);
}

void BinaryUploadAck::set_status(
    enterprise_connectors::ContentAnalysisAcknowledgement::Status status) {
  ack_.set_status(status);
}

void BinaryUploadAck::set_final_action(
    enterprise_connectors::ContentAnalysisAcknowledgement::FinalAction
        final_action) {
  ack_.set_final_action(final_action);
}

}  // namespace enterprise_connectors
