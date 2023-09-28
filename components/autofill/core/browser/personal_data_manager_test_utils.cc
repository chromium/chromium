// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/personal_data_manager_test_utils.h"

#include "base/test/gmock_callback_support.h"
#include "components/autofill/core/browser/personal_data_manager.h"

namespace autofill {

PersonalDataLoadedObserverMock::PersonalDataLoadedObserverMock() = default;
PersonalDataLoadedObserverMock::~PersonalDataLoadedObserverMock() = default;

PersonalDataProfileTaskWaiter::PersonalDataProfileTaskWaiter(
    PersonalDataManager& pdm) {
  scoped_observation_.Observe(&pdm);
  ON_CALL(mock_observer_, OnPersonalDataFinishedProfileTasks())
      .WillByDefault(base::test::RunClosure(run_loop_.QuitClosure()));
}

PersonalDataProfileTaskWaiter::~PersonalDataProfileTaskWaiter() = default;

void PersonalDataProfileTaskWaiter::Wait() && {
  run_loop_.Run();
}

}  // namespace autofill
