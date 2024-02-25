// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/personal_data_manager_observer_bridge.h"

#include "base/check.h"

namespace autofill {

PersonalDataManagerObserverBridge::PersonalDataManagerObserverBridge(
    id<PersonalDataManagerObserver> delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

PersonalDataManagerObserverBridge::~PersonalDataManagerObserverBridge() {
}

void PersonalDataManagerObserverBridge::OnPersonalDataChanged() {
  [delegate_ onPersonalDataChanged];
}

}  // namespace autofill
