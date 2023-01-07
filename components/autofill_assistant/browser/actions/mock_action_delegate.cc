// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"

namespace autofill_assistant {

using ::testing::Return;
using ::testing::ReturnRef;

MockActionDelegate::MockActionDelegate() {
  ON_CALL(*this, GetLogInfo).WillByDefault(ReturnRef(log_info_));
  ON_CALL(*this, GetElementStore).WillByDefault(Return(&fake_element_store_));
}

MockActionDelegate::~MockActionDelegate() = default;

}  // namespace autofill_assistant
