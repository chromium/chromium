// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CARD_UNMASK_CHALLENGE_OPTION_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CARD_UNMASK_CHALLENGE_OPTION_H_

namespace autofill {

// Indicates the type of challenge option used in card unmasking.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.ui.autofill
// GENERATED_JAVA_PREFIX_TO_STRIP: k
enum class CardUnmaskChallengeOptionType {
  // Default value, should never be used.
  kUnknownType = 0,
  // SMS OTP authentication.
  kSmsOtp = 1,
  kMaxValue = kSmsOtp,
};

// The struct used by Autofill components to represent a card unmask challenge
// option.
struct CardUnmaskChallengeOption {
  CardUnmaskChallengeOption() = default;
  CardUnmaskChallengeOption(const CardUnmaskChallengeOption&) = default;
  ~CardUnmaskChallengeOption() = default;
  CardUnmaskChallengeOption& operator=(const CardUnmaskChallengeOption&) =
      default;

  // The unique identifier for the challenge option.
  std::string id = std::string();

  // The type of the challenge option.
  CardUnmaskChallengeOptionType type =
      CardUnmaskChallengeOptionType::kUnknownType;

  // The user-facing text providing additional information for the challenge
  // option, such as the masked phone number that will receive an SMS, etc.
  std::u16string challenge_info = std::u16string();

  // The predetermined length of the OTP value.
  size_t otp_length = 0U;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CARD_UNMASK_CHALLENGE_OPTION_H_
