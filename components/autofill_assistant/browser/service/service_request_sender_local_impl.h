// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_SERVICE_REQUEST_SENDER_LOCAL_IMPL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_SERVICE_REQUEST_SENDER_LOCAL_IMPL_H_

#include <string>
#include "components/autofill_assistant/browser/service/service_request_sender.h"

namespace autofill_assistant {

// Implementation of a service request sender that serves a pre-configured,
// local response.
class ServiceRequestSenderLocalImpl : public ServiceRequestSender {
 public:
  ServiceRequestSenderLocalImpl(const std::string& response);
  ~ServiceRequestSenderLocalImpl() override;

  // This will always return status 200 and the response specified in the
  // constructor.
  // TODO(arbesser): Make this more flexible.
  void SendRequest(const GURL& url,
                   const std::string& request_body,
                   ServiceRequestSender::AuthMode auth_mode,
                   ResponseCallback callback,
                   RpcType rpc_type) override;

 private:
  std::string response_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_SERVICE_REQUEST_SENDER_LOCAL_IMPL_H_
