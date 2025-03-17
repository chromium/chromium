// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/clipboard/clipboard_history_url_title_fetcher_impl.h"

#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/user_manager/user_manager.h"

namespace {

// Returns the history service for the primary user profile iff the session has
// exactly one profile. Note that the logic of which profile(s) to query for
// browsing history, and under what conditions, may change in the future.
history::HistoryService* GetHistoryService() {
  content::BrowserContext* browser_context =
      ash::BrowserContextHelper::Get()->GetBrowserContextByUser(
          user_manager::UserManager::Get()->GetPrimaryUser());

  if (user_manager::UserManager::Get()->GetLoggedInUsers().size() != 1 ||
      !browser_context) {
    return nullptr;
  }

  return HistoryServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context),
      ServiceAccessType::EXPLICIT_ACCESS);
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
