// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_SERVICE_REQUEST_SENDER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_SERVICE_REQUEST_SENDER_H_

#include <string>

#include "base/callback.h"
#include "components/autofill_assistant/browser/service/rpc_type.h"
#include "url/gurl.h"

namespace autofill_assistant {

class ServiceRequestSender {
 public:
  // Contains information about the network response.
  struct ResponseInfo {
    // The number of bytes transmitted over the network, before decoding. Can be
    // -1 in case of interrupted downloads.
    int64_t encoded_body_length = 0;
  };

  using ResponseCallback =
      base::OnceCallback<void(int http_status,
                              const std::string& response,
                              const ResponseInfo& response_info)>;

  enum class AuthMode {
    // Requires an OAuth token for the request.
    OAUTH_STRICT,
    // Tries to get an OAuth token for the request but allows to fall back to
    // the API key if the fetch failed.
    OAUTH_WITH_API_KEY_FALLBACK,
    // Does not get an OAuth token and uses the API key directly.
    API_KEY
  };

  ServiceRequestSender();
  virtual ~ServiceRequestSender();

  // Sends |request_body| to |url|. Returns the http status code and the
  // response itself.
  virtual void SendRequest(const GURL& url,
                           const std::string& request_body,
                           AuthMode auth_mode,
                           ResponseCallback response_callback,
                           RpcType rpc_type) = 0;

  virtual void SetDisableRpcSigning(bool disable_rpc_signing) = 0;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_SERVICE_REQUEST_SENDER_H_
