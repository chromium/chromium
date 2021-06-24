// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/mock_web_controller.h"

namespace autofill_assistant {

MockWebController::MockWebController()
    : WebController(nullptr, nullptr, nullptr) {}
MockWebController::~MockWebController() {}

}  // namespace autofill_assistant
