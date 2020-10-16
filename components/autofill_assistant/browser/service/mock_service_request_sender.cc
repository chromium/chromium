// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/service/mock_service_request_sender.h"

namespace autofill_assistant {

MockServiceRequestSender::MockServiceRequestSender()
    : ServiceRequestSender(/* context = */ nullptr,
                           /* access_token_fetcher = */ nullptr,
                           /* loader_factory = */ nullptr,
                           /* api_key = */ std::string("fake_api_key"),
                           /* auth_enabled = */ false,
                           /* disable_auth_if_no_access_token = */ true) {}
MockServiceRequestSender::~MockServiceRequestSender() = default;

}  // namespace autofill_assistant
