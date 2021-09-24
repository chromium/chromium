// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CARD_UNMASK_CHALLENGE_OPTION_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CARD_UNMASK_CHALLENGE_OPTION_H_

namespace autofill {

// Indicates the type of challenge option used in card unmasking.
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
  std::string id;

  // The type of the challenge option.
  CardUnmaskChallengeOptionType type;

  // The user-facing text providing additional information for the challenge
  // option, such as the masked phone number that will receive an SMS, etc.
  std::u16string challenge_info;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CARD_UNMASK_CHALLENGE_OPTION_H_
