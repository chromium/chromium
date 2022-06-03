// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_MOCK_SERVICE_REQUEST_SENDER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_MOCK_SERVICE_REQUEST_SENDER_H_

#include <string>

#include "components/autofill_assistant/browser/service/service_request_sender.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace autofill_assistant {

class MockServiceRequestSender : public ServiceRequestSender {
 public:
  MockServiceRequestSender();
  ~MockServiceRequestSender() override;

  void SendRequest(const GURL& url,
                   const std::string& request_body,
                   ResponseCallback callback) override {
    OnSendRequest(url, request_body, callback);
  }

  MOCK_METHOD3(OnSendRequest,
               void(const GURL& url,
                    const std::string& request_body,
                    ResponseCallback& callback));
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_MOCK_SERVICE_REQUEST_SENDER_H_
