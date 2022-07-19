// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_PASSWORD_CHANGE_FEATURES_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_PASSWORD_CHANGE_FEATURES_H_

namespace base {
struct Feature;
}

namespace autofill_assistant {
namespace password_change {
namespace features {

extern const base::Feature
    kAutofillAssistantAPCLeakCheckOnSaveSubmittedPassword;

}  // namespace features
}  // namespace password_change
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_PASSWORD_CHANGE_FEATURES_H_
