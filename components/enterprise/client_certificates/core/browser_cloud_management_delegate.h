// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_BROWSER_CLOUD_MANAGEMENT_DELEGATE_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_BROWSER_CLOUD_MANAGEMENT_DELEGATE_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/enterprise/client_certificates/core/cloud_management_delegate.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"
#include "net/http/http_response_headers.h"

namespace enterprise_attestation {

class DMServerClient;

// Implementation of the CloudManagementDelegate interface for browser-level
// cloud management.
class BrowserCloudManagementDelegate : public CloudManagementDelegate {
 public:
  BrowserCloudManagementDelegate(
      std::unique_ptr<DMServerClient> dmserver_client);

  ~BrowserCloudManagementDelegate() override;

  std::optional<std::string> GetDMToken() const override;

  void UploadBrowserPublicKey(
      const enterprise_management::DeviceManagementRequest& upload_request,
      policy::DMServerJobConfiguration::Callback callback) override;

 private:
  std::unique_ptr<DMServerClient> dm_server_client_;
};

}  // namespace enterprise_attestation

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_BROWSER_CLOUD_MANAGEMENT_DELEGATE_H_
