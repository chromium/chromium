// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_OMNIBOX_AUTOFILL_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_OMNIBOX_AUTOFILL_DELEGATE_H_

#include <optional>

#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/scoped_autofill_managers_observation.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

class AutofillClient;
class AutofillDriver;

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
  void OnAutofillManagerStateChanged(
      AutofillManager& manager,
      AutofillDriver::LifecycleState previous,
      AutofillDriver::LifecycleState current) override;
  void OnAfterFormsSeen(AutofillManager& manager,
                        base::span<const FormGlobalId> updated_forms,
                        base::span<const FormGlobalId> removed_forms) override;

  void OnGetIntersectionObserverInfo(bool is_visible);

 private:
  // Returns `true` if `manager`'s AutofillDriver is active, has no parent, and
  // is not embedded. Returns `false` otherwise. Most OmniboxAutofillDelegate
  // functionality only wants to run on the outermost, main frame, active BAM.
  bool IsOutermostMainFrameActiveAutofillManager(AutofillManager& manager);

  // Checks if the given `field` is in the main frame.
  bool FieldIsInMainFrame(AutofillManager& manager,
                          const AutofillField& field) const;

  // If true, the OmniboxAutofillDelegate is likely waiting for the user to
  // scroll the candidate form into the viewport, so parsing logic to find
  // candidate forms should no longer be run.
  bool candidate_form_found_ = false;

  // The global ID of the form for which Omnibox Autofill should trigger.
  FormGlobalId trigger_form_global_id_;

  // The global ID of the field on which Omnibox Autofill should trigger. Note
  // that this is ensured to be of type CREDIT_CARD_NUMBER.
  FieldGlobalId trigger_field_global_id_;

  const raw_ref<AutofillClient> client_;

  ScopedAutofillManagersObservation autofill_managers_observation_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_OMNIBOX_AUTOFILL_DELEGATE_H_
