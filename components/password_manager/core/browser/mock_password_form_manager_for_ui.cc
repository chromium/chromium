// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/mock_password_form_manager_for_ui.h"

namespace password_manager {

MockPasswordFormManagerForUI::MockPasswordFormManagerForUI() {
  // TODO(crbug.com/538574948): Avoid defining pre-specified behavior in this
  // mock.
  ON_CALL(*this, IsFetchCompleted()).WillByDefault(testing::Return(true));
  ON_CALL(*this, IsPasswordUpdate()).WillByDefault(testing::Return(false));
}
MockPasswordFormManagerForUI::~MockPasswordFormManagerForUI() = default;

}  // namespace password_manager
