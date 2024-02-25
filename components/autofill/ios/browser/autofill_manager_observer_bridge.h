// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_MANAGER_OBSERVER_BRIDGE_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_MANAGER_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "components/autofill/core/browser/autofill_manager.h"

// Objective-C version of the AutofillManager::Observer interface. Only the
// methods that are currently in use in Objective-C are implemented.
@protocol AutofillManagerObserver <NSObject>

@optional

- (void)onAutofillManagerDestroyed:(autofill::AutofillManager&)manager;

- (void)onFieldTypesDetermined:(autofill::AutofillManager&)manager
                       forForm:(autofill::FormGlobalId)form
                    fromSource:
                        (autofill::AutofillManager::Observer::FieldTypeSource)
                            source;
@end

namespace autofill {

class AutofillManagerObserverBridge final : public AutofillManager::Observer {
 public:
  explicit AutofillManagerObserverBridge(id<AutofillManagerObserver> observer);

  AutofillManagerObserverBridge(const AutofillManagerObserverBridge&) = delete;
  AutofillManagerObserverBridge& operator=(
      const AutofillManagerObserverBridge&) = delete;

  ~AutofillManagerObserverBridge() final;

  // AutofillManager::Observer:
  void OnAutofillManagerDestroyed(AutofillManager& manager) override;
  void OnFieldTypesDetermined(AutofillManager& manager,
                              FormGlobalId form,
                              FieldTypeSource source) override;

 private:
  __weak id<AutofillManagerObserver> observer_ = nil;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_MANAGER_OBSERVER_BRIDGE_H_
