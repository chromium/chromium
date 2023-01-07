// Copyright 2020 The Chromium Authors
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
  // Note: |encoded_body_length| is set to 0 since nothing was sent over the
  // network.
  std::move(callback).Run(net::HTTP_OK, response_,
                          /* response_info = */ {});
}

void ServiceRequestSenderLocalImpl::SetDisableRpcSigning(
    bool disable_rpc_signing) {}

}  // namespace autofill_assistant
