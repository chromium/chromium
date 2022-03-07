// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/service/service_request_sender_local_impl.h"

#include "net/http/http_status_code.h"

namespace autofill_assistant {

ServiceRequestSenderLocalImpl::ServiceRequestSenderLocalImpl(
    const std::string& response)
    : response_(response) {}
ServiceRequestSenderLocalImpl::~ServiceRequestSenderLocalImpl() = default;

void ServiceRequestSenderLocalImpl::SendRequest(
    const GURL& url,
    const std::string& request_body,
    ServiceRequestSender::AuthMode auth_mode,
    ResponseCallback callback,
    RpcType rpc_type) {
  std::move(callback).Run(net::HTTP_OK, response_);
}

}  // namespace autofill_assistant
