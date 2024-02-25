// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/personal_data_manager_test_utils.h"

#include "base/test/gmock_callback_support.h"
#include "components/autofill/core/browser/personal_data_manager.h"

namespace autofill {

PersonalDataLoadedObserverMock::PersonalDataLoadedObserverMock() = default;
PersonalDataLoadedObserverMock::~PersonalDataLoadedObserverMock() = default;

PersonalDataChangedWaiter::PersonalDataChangedWaiter(PersonalDataManager& pdm) {
  scoped_observation_.Observe(&pdm);
  ON_CALL(mock_observer_, OnPersonalDataChanged())
      .WillByDefault(base::test::RunClosure(run_loop_.QuitClosure()));
}

PersonalDataChangedWaiter::~PersonalDataChangedWaiter() = default;

void PersonalDataChangedWaiter::Wait() && {
  run_loop_.Run();
}

void WaitForPendingDBTasks(AutofillWebDataService& webdata_service) {
  base::RunLoop run_loop;
  webdata_service.GetDBTaskRunner()->PostTask(FROM_HERE,
                                              run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace autofill
