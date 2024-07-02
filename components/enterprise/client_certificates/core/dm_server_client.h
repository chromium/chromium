// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_DM_SERVER_CLIENT_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_DM_SERVER_CLIENT_H_

#include <optional>
#include <string_view>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace enterprise_management {
class DeviceManagementRequest;
}  // namespace enterprise_management

namespace policy {
class DeviceManagementService;
}  // namespace policy

namespace enterprise_attestation {

// Client with a simple interface for sending a request to the DM server,
// abstracting away the networking implementation details.
class DMServerClient {
 public:
  // Visible for testing.
  static constexpr char kNetErrorHistogram[] =
      "Enterprise.DeviceTrust.PublicKeyUpload.URLLoaderNetError";

  static std::unique_ptr<DMServerClient> Create(
      raw_ptr<policy::DeviceManagementService> device_management_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Uploads browser public key to DM server. DM server retries in case of net
  // error.
  virtual void UploadBrowserPublicKey(
      const std::string& client_id,
      const std::string& dm_token,
      const std::optional<std::string>& profile_id,
      const enterprise_management::DeviceManagementRequest& upload_request,
      policy::DMServerJobConfiguration::Callback callback) = 0;

  virtual ~DMServerClient() = default;
};

}  // namespace enterprise_attestation

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_DM_SERVER_CLIENT_H_
