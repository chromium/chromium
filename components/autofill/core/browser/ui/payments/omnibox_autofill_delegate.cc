// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/omnibox_autofill_delegate.h"

#include "base/check_deref.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/scoped_autofill_managers_observation.h"
#include "components/autofill/core/browser/metrics/payments/omnibox_autofill_metrics.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

using autofill_metrics::OmniboxAutofillShowChipDecisionPart1;

OmniboxAutofillDelegate::OmniboxAutofillDelegate(AutofillClient* client)
    : client_(CHECK_DEREF(client)) {
  // While InitializationPolicy::kExpectNoPreexistingManagers would make more
  // sense for production, it fails in tests, likely because
  // TestPaymentsAutofillClient is lazily initialized.
  autofill_managers_observation_.Observe(
      client, autofill::ScopedAutofillManagersObservation::
                  InitializationPolicy::kObservePreexistingManagers);
}

OmniboxAutofillDelegate::~OmniboxAutofillDelegate() = default;

void OmniboxAutofillDelegate::OnFieldTypesDetermined(
    AutofillManager& manager,
    FormGlobalId form_id,
    AutofillManager::Observer::FieldTypeSource source,
    bool small_forms_were_parsed) {
  // Only run checks using the the outermost AutofillManager to avoid having
  // multiple managers triggering the logic flow at once.
  if (!IsOutermostMainFrameActiveAutofillManager(manager)) {
    // Don't log an `OmniboxAutofillShowChipDecisionPart1` entry here, because
    // learning how many non-outermost-active-main-frame BAMs exist on the page
    // is not useful information.
    return;
  }

  // More checks to follow as implementation continues...

  LogOmniboxAutofillShowChipDecisionPart1(
      OmniboxAutofillShowChipDecisionPart1::kSuccess);
}

bool OmniboxAutofillDelegate::IsOutermostMainFrameActiveAutofillManager(
    AutofillManager& manager) {
  return manager.driver().GetParent() == nullptr &&
         !manager.driver().IsEmbedded() && manager.driver().IsActive();
}

}  // namespace autofill
