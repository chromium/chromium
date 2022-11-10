// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CARD_UNMASK_CHALLENGE_OPTION_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CARD_UNMASK_CHALLENGE_OPTION_H_

#include <string>

namespace autofill {

// Indicates the type of challenge option used in card unmasking.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.ui.autofill
// GENERATED_JAVA_PREFIX_TO_STRIP: k
enum class CardUnmaskChallengeOptionType {
  // Default value, should never be used.
  kUnknownType = 0,
  // SMS OTP authentication.
  kSmsOtp = 1,
  // CVC authentication.
  kCvc = 2,
  kMaxValue = kCvc,
};

// Indicates the position of the CVC, for example the front or back of the
// user's card.
enum class CvcPosition {
  // Default value, should never be used.
  kUnknown = 0,
  // The CVC is on the front of the user's card.
  kFrontOfCard = 1,
  // The CVC is on the front of the user's card.
  kBackOfCard = 2,
  kMaxValue = kBackOfCard,
};

// The struct used by Autofill components to represent a card unmask challenge
// option. User must select a challenge option to unmask their credit card.
// Currently, only CVC and SMS OTP are supported.
struct CardUnmaskChallengeOption {
  // The unique identifier for the challenge option.
  std::string id = std::string();

  // The type of the challenge option.
  CardUnmaskChallengeOptionType type =
      CardUnmaskChallengeOptionType::kUnknownType;

  // The user-facing text providing additional information for the challenge
  // option, such as the masked phone number that will receive an SMS, etc.
  std::u16string challenge_info = std::u16string();

  // The predetermined length of the input of the challenge.
  size_t challenge_input_length = 0U;

  // The position of the CVC. Only present if `type` is `kCvc`.
  CvcPosition cvc_position = CvcPosition::kUnknown;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CARD_UNMASK_CHALLENGE_OPTION_H_
