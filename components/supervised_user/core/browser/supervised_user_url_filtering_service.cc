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

void SupervisedUserUrlFilteringService::GetFilteringBehavior(
    const GURL& url,
    bool skip_manual_parent_filter,
    SupervisedUserURLFilter::ResultCallback callback,
    const WebFilterMetricsOptions& options) const {
  supervised_user_service_->GetURLFilter()->GetFilteringBehaviorWithAsyncChecks(
      url, std::move(callback), skip_manual_parent_filter,
      options.filtering_context, options.transition_type);
}

// Version of the above method that for use in subframe context.
void SupervisedUserUrlFilteringService::GetFilteringBehaviorForSubFrame(
    const GURL& url,
    const GURL& main_frame_url,
    SupervisedUserURLFilter::ResultCallback callback,
    const WebFilterMetricsOptions& options) const {
  supervised_user_service_->GetURLFilter()
      ->GetFilteringBehaviorForSubFrameWithAsyncChecks(
          url, main_frame_url, std::move(callback), options.filtering_context,
          options.transition_type);
}
}  // namespace supervised_user
