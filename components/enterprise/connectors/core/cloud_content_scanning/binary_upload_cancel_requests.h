// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_BINARY_UPLOAD_CANCEL_REQUESTS_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_BINARY_UPLOAD_CANCEL_REQUESTS_H_

#include "components/enterprise/connectors/core/analysis_settings.h"

namespace enterprise_connectors {

// A class to encapsulate requests to cancel.  Any request that match the
// given criteria is canceled.  This is best effort only, in some cases
// requests may have already started and can no longer be canceled.
class BinaryUploadCancelRequests {
 public:
  explicit BinaryUploadCancelRequests(CloudOrLocalAnalysisSettings settings);
  virtual ~BinaryUploadCancelRequests();
  BinaryUploadCancelRequests(const BinaryUploadCancelRequests&) = delete;
  BinaryUploadCancelRequests& operator=(const BinaryUploadCancelRequests&) =
      delete;
  BinaryUploadCancelRequests(BinaryUploadCancelRequests&&) = delete;
  BinaryUploadCancelRequests& operator=(BinaryUploadCancelRequests&&) = delete;

  void set_user_action_id(const std::string& user_action_id);
  const std::string& get_user_action_id() const { return user_action_id_; }

  const CloudOrLocalAnalysisSettings& cloud_or_local_settings() const {
    return cloud_or_local_settings_;
  }

 private:
  std::string user_action_id_;

  // Settings used to determine how the request is used in the cloud or
  // locally.
  CloudOrLocalAnalysisSettings cloud_or_local_settings_;
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_BINARY_UPLOAD_CANCEL_REQUESTS_H_
