// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/client_policy_controller.h"

#include <utility>

#include "base/time/time.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_feature.h"

using LifetimeType = offline_pages::LifetimePolicy::LifetimeType;

namespace offline_pages {

ClientPolicyController::ClientPolicyController() {
  policies_.clear();
  // Manually defining client policies for bookmark and last_n.
  policies_.emplace(
      kBookmarkNamespace,
      MakePolicy(kBookmarkNamespace, LifetimeType::TEMPORARY,
                 base::TimeDelta::FromDays(7), kUnlimitedPages, 1));
  policies_.emplace(
      kLastNNamespace,
      OfflinePageClientPolicyBuilder(kLastNNamespace, LifetimeType::TEMPORARY,
                                     kUnlimitedPages, kUnlimitedPages)
          .SetExpirePeriod(base::TimeDelta::FromDays(30))
          .SetIsSupportedByRecentTabs(true)
          .SetIsRestrictedToTabFromClientId(true)
          .Build());
  policies_.emplace(
      kAsyncNamespace,
      OfflinePageClientPolicyBuilder(kAsyncNamespace, LifetimeType::PERSISTENT,
                                     kUnlimitedPages, kUnlimitedPages)
          .SetIsSupportedByDownload(true)
          .SetIsUserRequestedDownload(true)
          .SetIsRemovedOnCacheReset(false)
          .Build());
  policies_.emplace(
      kCCTNamespace,
      OfflinePageClientPolicyBuilder(kCCTNamespace, LifetimeType::TEMPORARY,
                                     kUnlimitedPages, 1)
          .SetExpirePeriod(base::TimeDelta::FromDays(2))
          .SetIsDisabledWhenPrefetchDisabled(true)
          .Build());
  policies_.emplace(kDownloadNamespace,
                    OfflinePageClientPolicyBuilder(
                        kDownloadNamespace, LifetimeType::PERSISTENT,
                        kUnlimitedPages, kUnlimitedPages)
                        .SetIsRemovedOnCacheReset(false)
                        .SetIsSupportedByDownload(true)
                        .SetIsUserRequestedDownload(true)
                        .Build());
  policies_.emplace(kNTPSuggestionsNamespace,
                    OfflinePageClientPolicyBuilder(
                        kNTPSuggestionsNamespace, LifetimeType::PERSISTENT,
                        kUnlimitedPages, kUnlimitedPages)
                        .SetIsSupportedByDownload(true)
                        .SetIsUserRequestedDownload(true)
                        .SetIsRemovedOnCacheReset(false)
                        .Build());
  policies_.emplace(
      kSuggestedArticlesNamespace,
      OfflinePageClientPolicyBuilder(kSuggestedArticlesNamespace,
                                     LifetimeType::TEMPORARY, kUnlimitedPages,
                                     kUnlimitedPages)
          .SetIsRemovedOnCacheReset(true)
          .SetIsDisabledWhenPrefetchDisabled(true)
          .SetExpirePeriod(base::TimeDelta::FromDays(30))
          .SetIsSupportedByDownload(IsPrefetchingOfflinePagesEnabled())
          .SetIsSuggested(true)
          .Build());
  policies_.emplace(kBrowserActionsNamespace,
                    OfflinePageClientPolicyBuilder(
                        kBrowserActionsNamespace, LifetimeType::PERSISTENT,
                        kUnlimitedPages, kUnlimitedPages)
                        .SetIsRemovedOnCacheReset(false)
                        .SetIsSupportedByDownload(true)
                        .SetIsUserRequestedDownload(true)
                        .SetShouldAllowDownload(true)
                        .Build());
  policies_.emplace(kLivePageSharingNamespace,
                    OfflinePageClientPolicyBuilder(kLivePageSharingNamespace,
                                                   LifetimeType::TEMPORARY,
                                                   kUnlimitedPages, 1)
                        .SetIsRemovedOnCacheReset(true)
                        .SetExpirePeriod(base::TimeDelta::FromHours(1))
                        .SetIsRestrictedToTabFromClientId(true)
                        .Build());
  policies_.emplace(
      kAutoAsyncNamespace,
      OfflinePageClientPolicyBuilder(
          kAutoAsyncNamespace, LifetimeType::TEMPORARY, kUnlimitedPages, 1)
          .SetIsRemovedOnCacheReset(true)
          .SetExpirePeriod(base::TimeDelta::FromDays(30))
          .SetIsUserRequestedDownload(false)
          .Build());

  // Fallback policy.
  policies_.emplace(kDefaultNamespace,
                    MakePolicy(kDefaultNamespace, LifetimeType::TEMPORARY,
                               base::TimeDelta::FromDays(1), 10, 1));
}

ClientPolicyController::~ClientPolicyController() {}

// static
const OfflinePageClientPolicy ClientPolicyController::MakePolicy(
    const std::string& name_space,
    LifetimeType lifetime_type,
    const base::TimeDelta& expire_period,
    size_t page_limit,
    size_t pages_allowed_per_url) {
  return OfflinePageClientPolicyBuilder(name_space, lifetime_type, page_limit,
                                        pages_allowed_per_url)
      .SetExpirePeriod(expire_period)
      .Build();
}

const OfflinePageClientPolicy& ClientPolicyController::GetPolicy(
    const std::string& name_space) const {
  const auto& iter = policies_.find(name_space);
  if (iter != policies_.end())
    return iter->second;
  // Fallback when the namespace isn't defined.
  return policies_.at(kDefaultNamespace);
}

std::vector<std::string> ClientPolicyController::GetAllNamespaces() const {
  std::vector<std::string> result;
  for (const auto& policy_item : policies_)
    result.emplace_back(policy_item.first);

  return result;
}

bool ClientPolicyController::IsRemovedOnCacheReset(
    const std::string& name_space) const {
  return GetPolicy(name_space).feature_policy.is_removed_on_cache_reset;
}

bool ClientPolicyController::IsSupportedByDownload(
    const std::string& name_space) const {
  return GetPolicy(name_space).feature_policy.is_supported_by_download;
}

bool ClientPolicyController::IsUserRequestedDownload(
    const std::string& name_space) const {
  return GetPolicy(name_space).feature_policy.is_user_requested_download;
}

const std::vector<std::string>&
ClientPolicyController::GetNamespacesRemovedOnCacheReset() const {
  if (cache_reset_namespace_cache_)
    return *cache_reset_namespace_cache_;

  cache_reset_namespace_cache_ = std::make_unique<std::vector<std::string>>();
  for (const auto& policy_item : policies_) {
    if (policy_item.second.feature_policy.is_removed_on_cache_reset)
      cache_reset_namespace_cache_->emplace_back(policy_item.first);
  }
  return *cache_reset_namespace_cache_;
}

const std::vector<std::string>&
ClientPolicyController::GetNamespacesSupportedByDownload() const {
  if (download_namespace_cache_)
    return *download_namespace_cache_;

  download_namespace_cache_ = std::make_unique<std::vector<std::string>>();
  for (const auto& policy_item : policies_) {
    if (policy_item.second.feature_policy.is_supported_by_download)
      download_namespace_cache_->emplace_back(policy_item.first);
  }
  return *download_namespace_cache_;
}

const std::vector<std::string>&
ClientPolicyController::GetNamespacesForUserRequestedDownload() const {
  if (user_requested_download_namespace_cache_)
    return *user_requested_download_namespace_cache_;

  user_requested_download_namespace_cache_ =
      std::make_unique<std::vector<std::string>>();
  for (const auto& policy_item : policies_) {
    if (policy_item.second.feature_policy.is_user_requested_download)
      user_requested_download_namespace_cache_->emplace_back(policy_item.first);
  }
  return *user_requested_download_namespace_cache_;
}

bool ClientPolicyController::IsShownAsRecentlyVisitedSite(
    const std::string& name_space) const {
  return GetPolicy(name_space).feature_policy.is_supported_by_recent_tabs;
}

const std::vector<std::string>&
ClientPolicyController::GetNamespacesShownAsRecentlyVisitedSite() const {
  if (recent_tab_namespace_cache_)
    return *recent_tab_namespace_cache_;

  recent_tab_namespace_cache_ = std::make_unique<std::vector<std::string>>();
  for (const auto& policy_item : policies_) {
    if (policy_item.second.feature_policy.is_supported_by_recent_tabs)
      recent_tab_namespace_cache_->emplace_back(policy_item.first);
  }

  return *recent_tab_namespace_cache_;
}

bool ClientPolicyController::IsRestrictedToTabFromClientId(
    const std::string& name_space) const {
  return GetPolicy(name_space)
      .feature_policy.is_restricted_to_tab_from_client_id;
}

const std::vector<std::string>&
ClientPolicyController::GetNamespacesRestrictedToTabFromClientId() const {
  if (restricted_to_tab_from_client_id_cache_)
    return *restricted_to_tab_from_client_id_cache_;

  restricted_to_tab_from_client_id_cache_ =
      std::make_unique<std::vector<std::string>>();
  for (const auto& policy_item : policies_) {
    if (policy_item.second.feature_policy.is_restricted_to_tab_from_client_id)
      restricted_to_tab_from_client_id_cache_->emplace_back(policy_item.first);
  }

  return *restricted_to_tab_from_client_id_cache_;
}

bool ClientPolicyController::IsDisabledWhenPrefetchDisabled(
    const std::string& name_space) const {
  return GetPolicy(name_space).feature_policy.disabled_when_prefetch_disabled;
}

const std::vector<std::string>&
ClientPolicyController::GetNamespacesDisabledWhenPrefetchDisabled() const {
  if (disabled_when_prefetch_disabled_cache_)
    return *disabled_when_prefetch_disabled_cache_;

  disabled_when_prefetch_disabled_cache_ =
      std::make_unique<std::vector<std::string>>();
  for (const auto& policy_item : policies_) {
    if (policy_item.second.feature_policy.disabled_when_prefetch_disabled)
      disabled_when_prefetch_disabled_cache_->emplace_back(policy_item.first);
  }

  return *disabled_when_prefetch_disabled_cache_;
}

bool ClientPolicyController::IsSuggested(const std::string& name_space) const {
  return GetPolicy(name_space).feature_policy.is_suggested;
}

bool ClientPolicyController::ShouldAllowDownloads(
    const std::string& name_space) const {
  return GetPolicy(name_space).feature_policy.should_allow_download;
}

void ClientPolicyController::AddPolicyForTest(
    const std::string& name_space,
    const OfflinePageClientPolicyBuilder& builder) {
  policies_.emplace(name_space, builder.Build());
}

}  // namespace offline_pages
