// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/browser_cloud_management_delegate.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/enterprise/client_certificates/core/dm_server_client.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace enterprise_attestation {

BrowserCloudManagementDelegate::BrowserCloudManagementDelegate(
    std::unique_ptr<DMServerClient> dm_server_client)
    : dm_server_client_(std::move(dm_server_client)) {
  CHECK(dm_server_client_);
}

BrowserCloudManagementDelegate::~BrowserCloudManagementDelegate() = default;

void BrowserCloudManagementDelegate::UploadBrowserPublicKey(
    const enterprise_management::DeviceManagementRequest& upload_request,
    policy::DMServerJobConfiguration::Callback callback) {
  dm_server_client_->UploadBrowserPublicKey(
      policy::BrowserDMTokenStorage::Get()->RetrieveClientId(),
      GetDMToken().value_or(""),
      /*profile_id=*/std::nullopt, std::move(upload_request),
      std::move(callback));
}

std::optional<std::string> BrowserCloudManagementDelegate::GetDMToken() const {
  auto dm_token = policy::BrowserDMTokenStorage::Get()->RetrieveDMToken();
  if (dm_token.is_valid()) {
    return dm_token.value();
  }
  return std::nullopt;
}

}  // namespace enterprise_attestation
