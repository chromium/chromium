// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_SCOPED_AUTOFILL_MANAGERS_OBSERVATION_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_SCOPED_AUTOFILL_MANAGERS_OBSERVATION_H_

#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/scoped_autofill_driver_ownees_observation.h"

namespace autofill {

namespace internal {
struct AutofillManagerFromDriver {
  AutofillManager& operator()(AutofillDriver& driver) const {
    return driver.GetAutofillManager();
  }
};
}  // namespace internal

// Observes all `AutofillManager`s associated with an `AutofillClient` (i.e., a
// `WebContents` on content embedders).
//
// Use it as follows:
// class MyClass : public AutofillManager::Observer {
//  public:
//   MyClass() {
//     managers_observation_.Observe(autofill_client());
//   }
//
//   // AutofillManager::Observer:
//   void OnFieldTypesDetermined(AutofillManager&,
//                               FormGlobalId,
//                               FieldTypeSource) override { ... }
//
//  private:
//   ScopedAutofillManagersObservation managers_observation_{this};
// };
//
// You do not have to worry about lifetimes:
// - If the observed `AutofillClient` is destroyed, the observation resets
//   itself.
// - If new managers are created or destroyed, observations for those are added
//   and removed automatically.
// - As any other `ScopedObservation`, it unregisters itself on destruction.
using ScopedAutofillManagersObservation =
    ScopedAutofillDriverOwneesObservation<AutofillManager,
                                          AutofillManager::Observer,
                                          internal::AutofillManagerFromDriver>;

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_SCOPED_AUTOFILL_MANAGERS_OBSERVATION_H_
