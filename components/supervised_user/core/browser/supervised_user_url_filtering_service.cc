// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_url_filtering_service.h"

#include "base/feature_list.h"
#include "components/supervised_user/core/browser/family_link_settings_service.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/common/features.h"

namespace supervised_user {

SupervisedUserUrlFilteringService::SupervisedUserUrlFilteringService(
    const SupervisedUserService& supervised_user_service,
    const FamilyLinkSettingsService& family_link_settings_service)
    : supervised_user_service_(supervised_user_service),
      family_link_settings_service_(family_link_settings_service) {}
SupervisedUserUrlFilteringService::~SupervisedUserUrlFilteringService() =
    default;

WebFilterType SupervisedUserUrlFilteringService::GetWebFilterType() const {
  if (base::FeatureList::IsEnabled(kSupervisedUserUseUrlFilteringService)) {
    return family_link_settings_service_->GetWebFilterType();
  }
  return supervised_user_service_->GetURLFilter()->GetWebFilterType();
}

SupervisedUserURLFilter::Result
SupervisedUserUrlFilteringService::GetFilteringBehavior(const GURL& url) const {
  return supervised_user_service_->GetURLFilter()->GetFilteringBehavior(url);
}

}  // namespace supervised_user
