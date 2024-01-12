// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_PERSONAL_DATA_MANAGER_OBSERVER_BRIDGE_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_PERSONAL_DATA_MANAGER_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "components/autofill/core/browser/personal_data_manager_observer.h"

// PersonalDataManagerObserver is used by PersonalDataManager to informs its
// client implemented in Objective-C when it has finished loading personal data
// from the web database.
@protocol PersonalDataManagerObserver<NSObject>

// Called when the PersonalDataManager changed in some way.
- (void)onPersonalDataChanged;

@end

namespace autofill {

// PersonalDataManagerObserverBridge forwards PersonalDataManager notification
// to an Objective-C delegate.
class PersonalDataManagerObserverBridge : public PersonalDataManagerObserver {
 public:
  explicit PersonalDataManagerObserverBridge(
      id<PersonalDataManagerObserver> delegate);

  PersonalDataManagerObserverBridge(const PersonalDataManagerObserverBridge&) =
      delete;
  PersonalDataManagerObserverBridge& operator=(
      const PersonalDataManagerObserverBridge&) = delete;

  ~PersonalDataManagerObserverBridge() override;

  // PersonalDataManagerObserver implementation.
  void OnPersonalDataChanged() override;

 private:
  __weak id<PersonalDataManagerObserver> delegate_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_PERSONAL_DATA_MANAGER_OBSERVER_BRIDGE_H_
