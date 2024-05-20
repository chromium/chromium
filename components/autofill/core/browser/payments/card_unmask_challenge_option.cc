// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"

namespace autofill {

Vcn3dsChallengeOptionMetadata::Vcn3dsChallengeOptionMetadata() = default;

Vcn3dsChallengeOptionMetadata::Vcn3dsChallengeOptionMetadata(
    const Vcn3dsChallengeOptionMetadata&) = default;

Vcn3dsChallengeOptionMetadata::Vcn3dsChallengeOptionMetadata(
    Vcn3dsChallengeOptionMetadata&&) = default;

Vcn3dsChallengeOptionMetadata& Vcn3dsChallengeOptionMetadata::operator=(
    const Vcn3dsChallengeOptionMetadata&) = default;

Vcn3dsChallengeOptionMetadata& Vcn3dsChallengeOptionMetadata::operator=(
    Vcn3dsChallengeOptionMetadata&&) = default;

Vcn3dsChallengeOptionMetadata::~Vcn3dsChallengeOptionMetadata() = default;

CardUnmaskChallengeOption::CardUnmaskChallengeOption(
    ChallengeOptionId id,
    CardUnmaskChallengeOptionType type,
    const std::u16string& challenge_info,
    const size_t& challenge_input_length,
    CvcPosition cvc_position)
    : id(id),
      type(type),
      challenge_info(challenge_info),
      challenge_input_length(challenge_input_length),
      cvc_position(cvc_position) {}

CardUnmaskChallengeOption::CardUnmaskChallengeOption() = default;

CardUnmaskChallengeOption::CardUnmaskChallengeOption(
    const CardUnmaskChallengeOption&) = default;

CardUnmaskChallengeOption& CardUnmaskChallengeOption::operator=(
    const CardUnmaskChallengeOption&) = default;

CardUnmaskChallengeOption::~CardUnmaskChallengeOption() = default;

}  // namespace autofill
