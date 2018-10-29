// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/mock_service.h"

#include "url/gurl.h"

namespace autofill_assistant {

MockService::MockService()
    : Service("api_key", GURL("http://fake"), nullptr, nullptr) {}
MockService::~MockService() {}

}  // namespace autofill_assistant
