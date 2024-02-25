// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_OBSERVER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_OBSERVER_H_

#include "components/history/core/browser/history_types.h"

namespace autofill {

// An interface the PersonalDataManager uses to notify its clients (observers)
// when it has finished loading personal data from the web database.  Register
// observers via PersonalDataManager::AddObserver.
class PersonalDataManagerObserver {
 public:
  // Notifies the observer that the PersonalDataManager changed in some way.
  // When multiple reads or writes are pending, `OnPersonalDataChanged()` is
  // only called once after all of them have finished.
  virtual void OnPersonalDataChanged() {}

 protected:
  virtual ~PersonalDataManagerObserver() {}
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_OBSERVER_H_
