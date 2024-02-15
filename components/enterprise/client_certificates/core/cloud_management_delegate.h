// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_CLOUD_MANAGEMENT_DELEGATE_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_CLOUD_MANAGEMENT_DELEGATE_H_

#include <optional>
#include <string>

namespace client_certificates {

// Delegate abstracting cloud management dependencies for the current context
// (e.g. user or browser).
class CloudManagementDelegate {
 public:
  virtual ~CloudManagementDelegate() = default;

  // Returns the cached DM token for the current context, or std::nullopt if
  // there is none.
  virtual std::optional<std::string> GetDMToken() const = 0;

  // Returns the DM server URL configured for the UploadBrowserPublicKey action,
  // or std::nullopt if its retrieval failed.
  virtual std::optional<std::string> GetUploadBrowserPublicKeyUrl() const = 0;
};

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_CLOUD_MANAGEMENT_DELEGATE_H_
