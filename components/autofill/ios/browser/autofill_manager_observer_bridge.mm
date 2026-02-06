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
    FieldTypeSource source,
    bool small_forms_were_parsed) {
  const SEL selector = @selector
      (onFieldTypesDetermined:forForm:fromSource:smallFormsWereParsed:);
  if (![observer_ respondsToSelector:selector]) {
    return;
  }
  [observer_ onFieldTypesDetermined:manager
                            forForm:form
                         fromSource:source
               smallFormsWereParsed:small_forms_were_parsed];
}

void AutofillManagerObserverBridge::OnAfterFormSubmitted(
    AutofillManager& manager,
    const FormData& form) {
  const SEL selector = @selector(onAfterFormSubmitted:formData:);
  if (![observer_ respondsToSelector:selector]) {
    return;
  }
  [observer_ onAfterFormSubmitted:manager formData:form];
}

}  // namespace autofill
