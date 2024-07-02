// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_CLOUD_MANAGEMENT_DELEGATE_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_CLOUD_MANAGEMENT_DELEGATE_H_

#include <optional>
#include <string>

#include "components/policy/core/common/cloud/dmserver_job_configurations.h"

namespace enterprise_attestation {

// Delegate abstracting cloud management dependencies for the current context
// (e.g. user or browser).
class CloudManagementDelegate {
 public:
  virtual ~CloudManagementDelegate() = default;

  // Returns the cached DM token for the current context, or std::nullopt if
  // there is none.
  virtual std::optional<std::string> GetDMToken() const = 0;

  // Uploads browser public key (from `upload_request`) to the DM server and
  // calls the callback when done. DM server retries uploading the key in case
  // of net error.
  virtual void UploadBrowserPublicKey(
      const enterprise_management::DeviceManagementRequest& upload_request,
      policy::DMServerJobConfiguration::Callback callback) = 0;
};

}  // namespace enterprise_attestation

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_CLOUD_MANAGEMENT_DELEGATE_H_
