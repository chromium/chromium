// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/clipboard/clipboard_history_url_title_fetcher_impl.h"

#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"

namespace {

// Arbitrarily chosen threshold after which we do not care to know the granular
// number of profiles added to a session.
constexpr int kMaxNumProfiles = 10;

// Returns the history service for the primary user profile iff the session has
// exactly one profile. Note that the logic of which profile(s) to query for
// browsing history, and under what conditions, may change in the future.
history::HistoryService* GetHistoryService() {
  int num_profiles = 0;
  for (auto* const profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    // Exclude non-user profiles, such as sign-in and lockscreen profiles, which
    // do not indicate use of multi-profile browsing.
    if (ash::IsUserBrowserContext(profile)) {
      ++num_profiles;
    }
  }
  base::UmaHistogramExactLinear(
      "Ash.ClipboardHistory.UrlTitleFetcher.NumProfiles", num_profiles,
      /*exclusive_max=*/kMaxNumProfiles + 1);

  auto* const profile = ProfileManager::GetPrimaryUserProfile();
  if (profile) {
    base::UmaHistogramBoolean(
        "Ash.ClipboardHistory.UrlTitleFetcher.IsPrimaryProfileActive",
        profile == ProfileManager::GetActiveUserProfile());
  }

  if (num_profiles != 1 || !profile) {
    return nullptr;
  }

  return HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
}

}  // namespace

ClipboardHistoryUrlTitleFetcherImpl::ClipboardHistoryUrlTitleFetcherImpl() =
    default;

ClipboardHistoryUrlTitleFetcherImpl::~ClipboardHistoryUrlTitleFetcherImpl() =
    default;

void ClipboardHistoryUrlTitleFetcherImpl::QueryHistory(
    const GURL& url,
    OnHistoryQueryCompleteCallback callback) {
  auto* const history_service = GetHistoryService();
  if (!history_service) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  // If `this` is destroyed, the `task_tracker_` will cancel this task
  // automatically and `OnHistoryQueryComplete()` will not be called. Therefore,
  // `Unretained(this)` is safe.
  history_service->QueryURL(
      url,
      /*want_visits=*/false,
      base::BindOnce(
          &ClipboardHistoryUrlTitleFetcherImpl::OnHistoryQueryComplete,
          base::Unretained(this), std::move(callback)),
      &task_tracker_);
}

void ClipboardHistoryUrlTitleFetcherImpl::OnHistoryQueryComplete(
    OnHistoryQueryCompleteCallback callback,
    history::QueryURLResult result) const {
  base::UmaHistogramBoolean("Ash.ClipboardHistory.UrlTitleFetcher.UrlFound",
                            result.success);
  std::move(callback).Run(
      result.success ? std::make_optional(result.row.title()) : std::nullopt);
}
