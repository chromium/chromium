// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/mock_service.h"
#include "components/autofill_assistant/browser/device_context.h"
#include "components/autofill_assistant/browser/trigger_context.h"

#include "url/gurl.h"

namespace autofill_assistant {

MockService::MockService()
    : ServiceImpl("api_key",
                  GURL("http://fake"),
                  nullptr,
                  nullptr,
                  "en_US",
                  "",
                  DeviceContext()) {}
MockService::~MockService() {}

}  // namespace autofill_assistant
