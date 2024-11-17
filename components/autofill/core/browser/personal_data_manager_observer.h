// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_OBSERVER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_OBSERVER_H_

namespace autofill {

// Deprecated. Use `AddressDataManager::Observer` or
// `PaymentsDataManager:`Observer` instead.
class PersonalDataManagerObserver {
 public:
  // Notifies the observer that the PersonalDataManager changed in some way.
  // When multiple reads or writes are pending, `OnPersonalDataChanged()` is
  // only called once after all of them have finished.
  virtual void OnPersonalDataChanged() {}

 protected:
  virtual ~PersonalDataManagerObserver() = default;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_OBSERVER_H_
