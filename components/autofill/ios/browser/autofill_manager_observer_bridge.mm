// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/autofill_manager_observer_bridge.h"

namespace autofill {

AutofillManagerObserverBridge::AutofillManagerObserverBridge(
    id<AutofillManagerObserver> observer)
    : observer_(observer) {}

AutofillManagerObserverBridge::~AutofillManagerObserverBridge() = default;

void AutofillManagerObserverBridge::OnAutofillManagerStateChanged(
    AutofillManager& manager,
    AutofillManager::LifecycleState old_state,
    AutofillManager::LifecycleState new_state) {
  const SEL selector = @selector(onAutofillManagerStateChanged:from:to:);
  if (![observer_ respondsToSelector:selector]) {
    return;
  }
  [observer_ onAutofillManagerStateChanged:manager from:old_state to:new_state];
}

void AutofillManagerObserverBridge::OnFieldTypesDetermined(
    AutofillManager& manager,
    FormGlobalId form,
    FieldTypeSource source) {
  const SEL selector = @selector(onFieldTypesDetermined:forForm:fromSource:);
  if (![observer_ respondsToSelector:selector]) {
    return;
  }
  [observer_ onFieldTypesDetermined:manager forForm:form fromSource:source];
}

}  // namespace autofill
