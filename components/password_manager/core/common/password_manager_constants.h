// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains constants specific to the Password Manager component.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_CONSTANTS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_CONSTANTS_H_

namespace password_manager::constants {

constexpr char kAutocompleteUsername[] = "username";
constexpr char kAutocompleteCurrentPassword[] = "current-password";
constexpr char kAutocompleteNewPassword[] = "new-password";
constexpr char kAutocompleteCreditCardPrefix[] = "cc-";
constexpr char kAutocompleteOneTimePassword[] = "one-time-code";
constexpr char kAutocompleteWebAuthn[] = "webauthn";

}  // namespace password_manager::constants

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_CONSTANTS_H_
