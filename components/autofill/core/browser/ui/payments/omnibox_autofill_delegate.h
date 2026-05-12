// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_OMNIBOX_AUTOFILL_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_OMNIBOX_AUTOFILL_DELEGATE_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/scoped_autofill_managers_observation.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

class AutofillClient;

class OmniboxAutofillDelegate : public AutofillManager::Observer {
 public:
  explicit OmniboxAutofillDelegate(AutofillClient* autofill_client);

  OmniboxAutofillDelegate(const OmniboxAutofillDelegate&) = delete;
  OmniboxAutofillDelegate& operator=(const OmniboxAutofillDelegate&) = delete;

  ~OmniboxAutofillDelegate() override;

  // AutofillManager::Observer:
  void OnFieldTypesDetermined(AutofillManager& manager,
                              FormGlobalId form,
                              AutofillManager::Observer::FieldTypeSource source,
                              bool small_forms_were_parsed) override;

  void OnGetIntersectionObserverInfo(bool is_visible);

 private:
  // Returns `true` if `manager`'s AutofillDriver is active, has no parent, and
  // is not embedded. Returns `false` otherwise. Most OmniboxAutofillDelegate
  // functionality only wants to run on the outermost, main frame, active BAM.
  bool IsOutermostMainFrameActiveAutofillManager(AutofillManager& manager);

  // Checks if the given `field` is in the main frame.
  bool FieldIsInMainFrame(AutofillManager& manager,
                          const AutofillField& field) const;

  const raw_ref<AutofillClient> client_;

  ScopedAutofillManagersObservation autofill_managers_observation_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_OMNIBOX_AUTOFILL_DELEGATE_H_
