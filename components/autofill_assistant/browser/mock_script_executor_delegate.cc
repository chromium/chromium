// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/mock_script_executor_delegate.h"

namespace autofill_assistant {

using ::testing::ReturnRef;

MockScriptExecutorDelegate::MockScriptExecutorDelegate() {
  ON_CALL(*this, GetSettings).WillByDefault(ReturnRef(client_settings_));
  ON_CALL(*this, GetLogInfo).WillByDefault(ReturnRef(log_info_));
  ON_CALL(*this, GetCurrentURL).WillByDefault(ReturnRef(default_url_));
}

MockScriptExecutorDelegate::~MockScriptExecutorDelegate() = default;

}  // namespace autofill_assistant
