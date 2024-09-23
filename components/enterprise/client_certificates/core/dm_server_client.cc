// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/dm_server_client.h"

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/uuid.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"

namespace enterprise_attestation {

class DMServerClientImpl : public DMServerClient {
 public:
  DMServerClientImpl(
      raw_ptr<policy::DeviceManagementService> device_management_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_factory);
  ~DMServerClientImpl() override;

  // Uploads browser public key to DM server. DM server retries in case of net
  // error.
  void UploadBrowserPublicKey(
      const std::string& client_id,
      const std::string& dm_token,
      const std::optional<std::string>& profile_id,
      const enterprise_management::DeviceManagementRequest& upload_request,
      policy::DMServerJobConfiguration::Callback callback) override;

 private:
  // Resets upload_request_job_, logs the error if any, and calls the
  // callback(result).
  void OnUploadingPublicKeyCompleted(
      const std::string& job_key,
      policy::DMServerJobConfiguration::Callback callback,
      const policy::DMServerJobResult result);

  const raw_ptr<policy::DeviceManagementService> device_management_service_ =
      nullptr;

  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Jobs that send a request to DM server for uploading a public key. We need
  // more than one job to be able to support parallel calls.
  base::flat_map<std::string,
                 std::unique_ptr<policy::DeviceManagementService::Job>>
      upload_public_key_jobs_;

  base::WeakPtrFactory<DMServerClientImpl> weak_factory_{this};
};

// static
std::unique_ptr<DMServerClient> DMServerClient::Create(
    raw_ptr<policy::DeviceManagementService> device_management_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  return std::make_unique<DMServerClientImpl>(device_management_service,
                                              std::move(url_loader_factory));
}

DMServerClientImpl::DMServerClientImpl(
    raw_ptr<policy::DeviceManagementService> device_management_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : device_management_service_(device_management_service),
      url_loader_factory_(std::move(url_loader_factory)) {
  CHECK(device_management_service_);
  CHECK(url_loader_factory_);
}

DMServerClientImpl::~DMServerClientImpl() = default;

void DMServerClientImpl::OnUploadingPublicKeyCompleted(
    const std::string& job_key,
    policy::DMServerJobConfiguration::Callback callback,
    const policy::DMServerJobResult result) {
  // Reset the job pointer and remove the entry from the map.
  upload_public_key_jobs_.erase(job_key);

  if (result.net_error != net::OK) {
    base::UmaHistogramSparse(kNetErrorHistogram, result.net_error);
  }

  std::move(callback).Run(result);
}

void DMServerClientImpl::UploadBrowserPublicKey(
    const std::string& client_id,
    const std::string& dm_token,
    const std::optional<std::string>& profile_id,
    const enterprise_management::DeviceManagementRequest& upload_request,
    policy::DMServerJobConfiguration::Callback callback) {
  if (!device_management_service_) {
    std::move(callback).Run(policy::DMServerJobResult{
        /* job */ nullptr, net::OK,
        policy::DeviceManagementStatus::DM_STATUS_REQUEST_FAILED,
        /* http response code */ 0,
        enterprise_management::DeviceManagementResponse()});
    return;
  }

  if (dm_token.empty()) {
    std::move(callback).Run(policy::DMServerJobResult{
        /* job */ nullptr, net::OK,
        policy::DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID,
        /* http response code */ 0,
        enterprise_management::DeviceManagementResponse()});
    return;
  }

  if (client_id.empty()) {
    std::move(callback).Run(policy::DMServerJobResult{
        /* job */ nullptr, net::OK, policy::DM_STATUS_REQUEST_INVALID,
        /* http response code */ 0,
        enterprise_management::DeviceManagementResponse()});
    return;
  }

  auto params = policy::DMServerJobConfiguration::CreateParams::WithoutClient(
      policy::DeviceManagementService::JobConfiguration::
          TYPE_BROWSER_UPLOAD_PUBLIC_KEY,
      device_management_service_, client_id, url_loader_factory_);

  params.critical = true;
  params.auth_data = policy::DMAuth::FromDMToken(dm_token);

  auto job_key = base::Uuid::GenerateRandomV4().AsLowercaseString();
  params.callback =
      base::BindOnce(&DMServerClientImpl::OnUploadingPublicKeyCompleted,
                     weak_factory_.GetWeakPtr(), job_key, std::move(callback));

  if (profile_id.has_value()) {
    params.profile_id = profile_id.value();
  }

  auto config =
      std::make_unique<policy::DMServerJobConfiguration>(std::move(params));

  config->request()->mutable_browser_public_key_upload_request()->CopyFrom(
      upload_request.browser_public_key_upload_request());

  upload_public_key_jobs_[job_key] =
      device_management_service_->CreateJob(std::move(config));
}

}  // namespace enterprise_attestation
