// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/personal_data_manager_observer_bridge.h"

#include "base/check.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

void PersonalDataManagerObserverBridge::OnInsufficientFormData() {
  if ([delegate_ respondsToSelector:@selector(onInsufficientFormData)])
    [delegate_ onInsufficientFormData];
}

}  // namespace autofill
