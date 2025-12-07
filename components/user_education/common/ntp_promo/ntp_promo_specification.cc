// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/ntp_promo/ntp_promo_specification.h"

#include <utility>

#include "components/user_education/common/user_education_metadata.h"

namespace user_education {

NtpPromoContent::NtpPromoContent(const NtpPromoContent&) = default;
NtpPromoContent::NtpPromoContent(NtpPromoContent&&) noexcept = default;
NtpPromoContent::~NtpPromoContent() = default;

NtpPromoContent::NtpPromoContent(std::string_view icon_name,
                                 int body_text_string_id,
                                 int action_button_text_string_id)
    : icon_name_(icon_name),
      body_text_string_id_(body_text_string_id),
      action_button_text_string_id_(action_button_text_string_id) {}

NtpPromoSpecification::~NtpPromoSpecification() = default;
NtpPromoSpecification::NtpPromoSpecification(NtpPromoSpecification&&) noexcept =
    default;

NtpPromoSpecification::NtpPromoSpecification(
    NtpPromoIdentifier id,
    NtpPromoContent content,
    EligibilityCallback eligibility_callback,
    ShowCallback show_callback,
    ActionCallback action_callback,
    base::flat_set<NtpPromoIdentifier> show_after,
    Metadata metadata)
    : id_(id),
      content_(std::move(content)),
      eligibility_callback_(eligibility_callback),
      show_callback_(show_callback),
      action_callback_(action_callback),
      show_after_(std::move(show_after)),
      metadata_(std::move(metadata)) {}

}  // namespace user_education
