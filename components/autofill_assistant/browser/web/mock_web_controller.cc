// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/mock_web_controller.h"

namespace autofill_assistant {

MockWebController::MockWebController()
    : WebController(/* web_contents= */ nullptr,
                    /* devtools_client= */ nullptr,
                    /* user_data= */ nullptr,
                    /* log_info= */ nullptr,
                    /* annotate_dom_model_service= */ nullptr) {}
MockWebController::~MockWebController() {}

}  // namespace autofill_assistant
