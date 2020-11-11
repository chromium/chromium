// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_SERVICE_REQUEST_SENDER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_SERVICE_REQUEST_SENDER_H_

#include <string>

#include "base/callback.h"
#include "url/gurl.h"

namespace autofill_assistant {

class ServiceRequestSender {
 public:
  using ResponseCallback =
      base::OnceCallback<void(int http_status, const std::string& response)>;

  ServiceRequestSender();
  virtual ~ServiceRequestSender();

  // Sends |request_body| to |url|. Returns the http status code and the
  // response itself.
  virtual void SendRequest(const GURL& url,
                           const std::string& request_body,
                           ResponseCallback callback) = 0;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_SERVICE_REQUEST_SENDER_H_
