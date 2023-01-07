// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/public/password_change/features.h"

#include "base/feature_list.h"

namespace autofill_assistant {
namespace password_change {
namespace features {

// Decides whether leak checks may be performed on saving a manually
// submitted password in an automated password change flow.
BASE_FEATURE(kAutofillAssistantAPCLeakCheckOnSaveSubmittedPassword,
             "AutofillAssistantAPCLeakCheckOnSaveSubmittedPassword",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
}  // namespace password_change
}  // namespace autofill_assistant
