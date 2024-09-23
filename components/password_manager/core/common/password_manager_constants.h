// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains constants specific to the Password Manager component.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_CONSTANTS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_CONSTANTS_H_

#include "base/time/time.h"

namespace password_manager::constants {

// The character used to obfuscate password labels.
inline constexpr char16_t kPasswordReplacementChar = 0x2022;

inline constexpr char kAutocompleteUsername[] = "username";
inline constexpr char kAutocompleteCurrentPassword[] = "current-password";
inline constexpr char kAutocompleteNewPassword[] = "new-password";
inline constexpr char kAutocompleteCreditCardPrefix[] = "cc-";
inline constexpr char kAutocompleteOneTimePassword[] = "one-time-code";
inline constexpr char kAutocompleteWebAuthn[] = "webauthn";

inline constexpr int kMaxPasswordNoteLength = 1000;
inline constexpr int kMaxPasswordsPerCSVFile = 3000;

inline constexpr base::TimeDelta kPasswordManagerAuthValidity =
    base::Minutes(5);

// Password manager specific regexes are defined below.

// Form parsing related regexes, used to rule out irrelevant fields (SSN, OTP,
// etc) based on their label or name attribute.
inline constexpr char16_t kSocialSecurityRe[] =
    u"ssn|social.?security.?(num(ber)?|#)*";
inline constexpr char16_t kOneTimePwdRe[] =
    // "One time" is good signal that it is an OTP field.
    u"one.?time|"
    // The main tokens are good signals, but they are short, require word
    // boundaries around them.
    u"(?:\\b|_)(?:otp|otc|totp|sms|2fa|mfa)(?:\\b|_)|"
    // Alternatively, require companion tokens before or after the main tokens.
    u"(?:otp|otc|totp|sms|2fa|mfa).?(?:code|token|input|val|pin|login|verif|"
    u"pass|pwd|psw|auth|field)|"
    u"(?:verif(?:y|ication)?|email|phone|text|login|input|txt|user).?(?:otp|"
    u"otc|totp|sms|2fa|mfa)|"
    // Sometimes the main tokens are combined with each other.
    u"sms.?otp|mfa.?otp|"
    // "code" is not so strong signal as the main tokens, but in combination
    // with "verification" and its variations it is.
    u"verif(?:y|ication)?.?code|(?:\\b|_)vcode|"
    // 'Second factor' and its variations are good signals.
    u"(?:second|two|2).?factor|"
    // A couple of custom strings that are usually OTP fields.
    u"wfls-token|email_code";

// Matches strings that consist of one repeated non-alphanumeric symbol,
// that is likely a result of the website modifying the value to hide it.
// This is run against the value of the field to filter out irrelevant fields.
inline constexpr char16_t kHiddenValueRe[] = u"^(\\W)\\1+$";

// Regexes used for crowdsourcing. They heuristically determine the
// `AutofillUploadContents::ValueType` of the user's input on-upload.
inline constexpr char16_t kEmailValueRe[] =
    u"^[a-zA-Z0-9.!#$%&’*+/=?^_`{|}~-]+@[a-zA-Z0-9-]+(?:\\.[a-zA-Z0-9-]+)*$";
inline constexpr char16_t kPhoneValueRe[] = u"^[0-9()+-]{6,25}$";
inline constexpr char16_t kUsernameLikeValueRe[] = u"[A-Za-z0-9_\\-.]{7,30}";

inline constexpr char16_t kSearch[] = u"search";

}  // namespace password_manager::constants

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_CONSTANTS_H_
