// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_user_data.h"

#include "base/rand_util.h"

namespace previews {

const void* const kPreviewsUserDataKey = &kPreviewsUserDataKey;

PreviewsUserData::PreviewsUserData(uint64_t page_id)
    : page_id_(page_id),
      random_coin_flip_for_navigation_(base::RandInt(0, 1)),
      server_lite_page_info_(nullptr) {}

PreviewsUserData::~PreviewsUserData() {}

PreviewsUserData::PreviewsUserData(const PreviewsUserData& other)
    : page_id_(other.page_id_),
      random_coin_flip_for_navigation_(other.random_coin_flip_for_navigation_),
      navigation_ect_(other.navigation_ect_),
      data_savings_inflation_percent_(other.data_savings_inflation_percent_),
      cache_control_no_transform_directive_(
          other.cache_control_no_transform_directive_),
      offline_preview_used_(other.offline_preview_used_),
      black_listed_for_lite_page_(other.black_listed_for_lite_page_),
      committed_previews_type_without_holdback_(
          other.committed_previews_type_without_holdback_),
      allowed_previews_state_without_holdback_(
          other.allowed_previews_state_without_holdback_),
      committed_previews_state_without_holdback_(
          other.committed_previews_state_without_holdback_),
      coin_flip_holdback_result_(other.coin_flip_holdback_result_),
      preview_eligibility_reasons_(other.preview_eligibility_reasons_),
      serialized_hint_version_string_(other.serialized_hint_version_string_) {
  if (other.server_lite_page_info_) {
    server_lite_page_info_ =
        std::make_unique<ServerLitePageInfo>(*other.server_lite_page_info_);
  }
}

void PreviewsUserData::SetCommittedPreviewsType(
    previews::PreviewsType previews_type) {
  DCHECK(committed_previews_type_without_holdback_ == PreviewsType::NONE);
  committed_previews_type_without_holdback_ = previews_type;
}

void PreviewsUserData::SetCommittedPreviewsTypeForTesting(
    previews::PreviewsType previews_type) {
  committed_previews_type_without_holdback_ = previews_type;
}

bool PreviewsUserData::CoinFlipForNavigation() const {
  if (params::ShouldOverrideNavigationCoinFlipToHoldback())
    return true;

  if (params::ShouldOverrideNavigationCoinFlipToAllowed())
    return false;

  return random_coin_flip_for_navigation_;
}

void PreviewsUserData::SetEligibilityReasonForPreview(
    PreviewsType preview,
    PreviewsEligibilityReason reason) {
  preview_eligibility_reasons_[preview] = reason;
}

base::Optional<PreviewsEligibilityReason>
PreviewsUserData::EligibilityReasonForPreview(PreviewsType preview) {
  auto iter = preview_eligibility_reasons_.find(preview);
  if (iter == preview_eligibility_reasons_.end())
    return base::nullopt;
  return iter->second;
}

bool PreviewsUserData::HasCommittedPreviewsType() const {
  return CommittedPreviewsType() != PreviewsType::NONE;
}

previews::PreviewsType PreviewsUserData::PreHoldbackCommittedPreviewsType()
    const {
  return committed_previews_type_without_holdback_;
}

previews::PreviewsType PreviewsUserData::CommittedPreviewsType() const {
  if (coin_flip_holdback_result_ == CoinFlipHoldbackResult::kHoldback)
    return PreviewsType::NONE;
  return committed_previews_type_without_holdback_;
}

content::PreviewsState PreviewsUserData::PreHoldbackAllowedPreviewsState()
    const {
  return allowed_previews_state_without_holdback_;
}

content::PreviewsState PreviewsUserData::AllowedPreviewsState() const {
  if (coin_flip_holdback_result_ == CoinFlipHoldbackResult::kHoldback)
    return content::PREVIEWS_OFF;
  return allowed_previews_state_without_holdback_;
}

content::PreviewsState PreviewsUserData::PreHoldbackCommittedPreviewsState()
    const {
  return committed_previews_state_without_holdback_;
}

content::PreviewsState PreviewsUserData::CommittedPreviewsState() const {
  if (coin_flip_holdback_result_ == CoinFlipHoldbackResult::kHoldback)
    return content::PREVIEWS_OFF;
  return committed_previews_state_without_holdback_;
}

}  // namespace previews
