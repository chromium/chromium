// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_UTIL_H_

namespace autofill {
struct FormData;
}  // namespace autofill

namespace password_manager::util {

// Returns whether this form is recognized as a credential form by the renderer.
// If is the case iff it has at least one field of type="password", a text field
// with autocomplete="username", or a textfield with autocomplete="webauthn".
bool IsRendererRecognizedCredentialForm(const autofill::FormData& form);

}  // namespace password_manager::util

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_UTIL_H_
