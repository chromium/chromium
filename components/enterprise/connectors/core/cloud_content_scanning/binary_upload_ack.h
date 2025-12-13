// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_BINARY_UPLOAD_ACK_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_BINARY_UPLOAD_ACK_H_

#include "components/enterprise/connectors/core/analysis_settings.h"

namespace enterprise_connectors {

// A class to encapsulate the a request acknowledgement. This class will
// provide all the functionality needed to generate a
// ContentAnalysisAcknowledgement.
class BinaryUploadAck {
 public:
  explicit BinaryUploadAck(CloudOrLocalAnalysisSettings settings);
  virtual ~BinaryUploadAck();
  BinaryUploadAck(const BinaryUploadAck&) = delete;
  BinaryUploadAck& operator=(const BinaryUploadAck&) = delete;
  BinaryUploadAck(BinaryUploadAck&&) = delete;
  BinaryUploadAck& operator=(BinaryUploadAck&&) = delete;

  void set_request_token(const std::string& token);
  void set_status(ContentAnalysisAcknowledgement::Status status);
  void set_final_action(
      ContentAnalysisAcknowledgement::FinalAction final_action);

  const CloudOrLocalAnalysisSettings& cloud_or_local_settings() const {
    return cloud_or_local_settings_;
  }

  const ContentAnalysisAcknowledgement& ack() const { return ack_; }

 private:
  ContentAnalysisAcknowledgement ack_;

  // Settings used to determine how the request is used in the cloud or
  // locally.
  CloudOrLocalAnalysisSettings cloud_or_local_settings_;
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_BINARY_UPLOAD_ACK_H_
