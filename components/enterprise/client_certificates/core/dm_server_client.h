// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_DM_SERVER_CLIENT_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_DM_SERVER_CLIENT_H_

#include <optional>
#include <string_view>

#include "base/functional/callback_forward.h"
#include "url/gurl.h"

namespace enterprise_management {
class DeviceManagementRequest;
class DeviceManagementResponse;
}  // namespace enterprise_management

namespace client_certificates {

// Client with a simple interface for sending a request to the DM server,
// abstracting away the networking implementation details.
class DMServerClient {
 public:
  virtual ~DMServerClient() = default;

  using SendRequestCallback = base::OnceCallback<void(
      int,
      std::optional<enterprise_management::DeviceManagementResponse>)>;

  // Sends `request_body` to the DM server at `url` using `dm_token` as auth
  // token. Invokes `callback` upon receiving the response with the HTTP status
  // code and body.
  virtual void SendRequest(
      const GURL& url,
      std::string_view dm_token,
      const enterprise_management::DeviceManagementRequest& request_body,
      SendRequestCallback callback) = 0;
};

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_DM_SERVER_CLIENT_H_
