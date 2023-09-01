// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/autofill_manager_observer_bridge.h"

namespace autofill {

AutofillManagerObserverBridge::AutofillManagerObserverBridge(
    id<AutofillManagerObserver> observer)
    : observer_(observer) {}

AutofillManagerObserverBridge::~AutofillManagerObserverBridge() = default;

void AutofillManagerObserverBridge::OnAutofillManagerDestroyed(
    AutofillManager& manager) {
  const SEL selector = @selector(onAutofillManagerDestroyed:);
  if (![observer_ respondsToSelector:selector]) {
    return;
  }
  [observer_ onAutofillManagerDestroyed:manager];
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
