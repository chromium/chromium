// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_UTIL_H_

#include <string>

namespace autofill {
class FormData;
}  // namespace autofill

namespace password_manager::util {

// Returns whether this form is recognized as a credential form by the renderer.
// If is the case iff it has at least one field of type="password", a text field
// with autocomplete="username", or a textfield with autocomplete="webauthn".
bool IsRendererRecognizedCredentialForm(const autofill::FormData& form);

// Returns whether field attributes allow to consider it as a single
// username field (e.g. don't indicate it's a search field).
bool CanFieldBeConsideredAsSingleUsername(const std::u16string& name,
                                          const std::u16string& id,
                                          const std::u16string& label);

// Returns whether the field value allows to consider it as a single
// username field.
bool CanValueBeConsideredAsSingleUsername(const std::u16string& value);

// Returns true if the field attributes indicate an OTP field.
bool IsLikelyOtp(std::u16string_view name,
                 std::u16string_view id,
                 std::string_view autocomplete);

}  // namespace password_manager::util

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_UTIL_H_
