// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"

#include "components/autofill_assistant/browser/mock_run_once_callback.h"
#include "url/gurl.h"

using ::testing::_;

namespace autofill_assistant {

MockActionDelegate::MockActionDelegate() = default;
MockActionDelegate::~MockActionDelegate() = default;

}  // namespace autofill_assistant
