// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains constants specific to the Password Manager component.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_CONSTANTS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_CONSTANTS_H_

namespace password_manager::constants {

inline constexpr char kAutocompleteUsername[] = "username";
inline constexpr char kAutocompleteCurrentPassword[] = "current-password";
inline constexpr char kAutocompleteNewPassword[] = "new-password";
inline constexpr char kAutocompleteCreditCardPrefix[] = "cc-";
inline constexpr char kAutocompleteOneTimePassword[] = "one-time-code";
inline constexpr char kAutocompleteWebAuthn[] = "webauthn";

inline constexpr int kMaxPasswordNoteLength = 1000;
inline constexpr int kMaxPasswordsPerCSVFile = 3000;

}  // namespace password_manager::constants

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_CONSTANTS_H_
