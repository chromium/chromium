// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/clipboard_history_url_title_fetcher_impl.h"

#include <string>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

// Returns the history service for the primary user profile. Note that the logic
// of which profile(s) to query for browsing history, and under what conditions,
// may change in the future.
history::HistoryService* GetHistoryService() {
  auto* const profile = ProfileManager::GetPrimaryUserProfile();
  if (!profile) {
    return nullptr;
  }

  base::UmaHistogramBoolean(
      "Ash.ClipboardHistory.UrlTitleFetcher.IsPrimaryProfileActive",
      profile == ProfileManager::GetActiveUserProfile());

  // TODO(http://b/267694762): Enforce the constraint that the primary profile's
  // browsing history can only be queried if the session has just one profile.

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
    std::move(callback).Run(absl::nullopt);
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
  std::move(callback).Run(
      result.success ? absl::make_optional(result.row.title()) : absl::nullopt);
}
