// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CARD_UNMASK_CHALLENGE_OPTION_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CARD_UNMASK_CHALLENGE_OPTION_H_

#include <optional>
#include <string>

#include "base/types/strong_alias.h"
#include "url/gurl.h"

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
  // Email OTP authentication.
  kEmailOtp = 3,
  // 3DS authentication.
  kThreeDomainSecure = 4,
  kMaxValue = kThreeDomainSecure,
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

// Metadata from the server related to a VCN 3DS challenge option.
struct Vcn3dsChallengeOptionMetadata {
  Vcn3dsChallengeOptionMetadata();
  Vcn3dsChallengeOptionMetadata(const Vcn3dsChallengeOptionMetadata&);
  Vcn3dsChallengeOptionMetadata(Vcn3dsChallengeOptionMetadata&&);
  Vcn3dsChallengeOptionMetadata& operator=(
      const Vcn3dsChallengeOptionMetadata&);
  Vcn3dsChallengeOptionMetadata& operator=(Vcn3dsChallengeOptionMetadata&&);
  ~Vcn3dsChallengeOptionMetadata();

  // URL that will be opened in the VCN 3DS pop-up.
  GURL url_to_open;

  // Name of the success query param. On every navigation inside of a VCN 3DS
  // pop-up, this query param will be searched for. If found, it signals to
  // Chrome that the authentication inside of the pop-up was successful. The
  // value of the query param will be the context token that must be sent back
  // to the server in the second UnmaskCardRequest call.
  std::string success_query_param_name;

  // Name of the failure query param. On every navigation inside of a VCN 3DS
  // pop-up, this query param will be searched for. If found, it signals to
  // Chrome that the authentication inside of the pop-up was a failure.
  std::string failure_query_param_name;
};

// The struct used by Autofill components to represent a card unmask challenge
// option. User must select a challenge option to unmask their credit card.
// Currently, only CVC and SMS OTP are supported.
struct CardUnmaskChallengeOption {
  // The challenge option ID is a unique identifier generated in the Payments
  // server and is used to distinguish challenge options from one another.
  using ChallengeOptionId =
      base::StrongAlias<class SelectedChallengeOptionIdTag, std::string>;

  CardUnmaskChallengeOption(ChallengeOptionId id,
                            CardUnmaskChallengeOptionType type,
                            const std::u16string& challenge_info,
                            const size_t& challenge_input_length,
                            CvcPosition cvc_position = CvcPosition::kUnknown);

  CardUnmaskChallengeOption();
  CardUnmaskChallengeOption(const CardUnmaskChallengeOption&);
  CardUnmaskChallengeOption& operator=(const CardUnmaskChallengeOption&);
  ~CardUnmaskChallengeOption();

  ChallengeOptionId id = ChallengeOptionId();

  // The type of the challenge option.
  CardUnmaskChallengeOptionType type =
      CardUnmaskChallengeOptionType::kUnknownType;

  // The user-facing text providing additional information for the challenge
  // option, such as the masked phone number that will receive an SMS, etc.
  std::u16string challenge_info = std::u16string();

  // Only present if `type` is
  // `CardUnmaskChallengeOptionType::kThreeDomainSecure`.
  std::optional<Vcn3dsChallengeOptionMetadata> vcn_3ds_metadata;

  // The predetermined length of the input of the challenge.
  size_t challenge_input_length = 0U;

  // The position of the CVC. Only present if `type` is `kCvc`.
  CvcPosition cvc_position = CvcPosition::kUnknown;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CARD_UNMASK_CHALLENGE_OPTION_H_
