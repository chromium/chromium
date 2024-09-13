// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_PASSWORD_FORM_CLASSIFICATION_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_PASSWORD_FORM_CLASSIFICATION_UTIL_H_

#include "components/autofill/core/browser/password_form_classification.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {
class AutofillManager;
}  // namespace autofill

namespace password_manager {

// Returns the heuristics predictions for the renderer form to which
// `field_id` belongs inside the form with `form_id`. The browser form with
// `form_id` is decomposed into renderer forms prior to running Password
// Manager heuristics.
// If the form cannot be found, `PasswordFormClassification::kNoPasswordForm` is
// returned.
autofill::PasswordFormClassification ClassifyAsPasswordForm(
    autofill::AutofillManager& manager,
    autofill::FormGlobalId form_id,
    autofill::FieldGlobalId field_id);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_PASSWORD_FORM_CLASSIFICATION_UTIL_H_
