// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/picker/picker_link_suggester.h"

#include "ash/constants/ash_features.h"
#include "ash/picker/picker_search_result.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "base/barrier_callback.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/history/core/browser/history_service.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"

namespace {

constexpr int kRecentDayRange = 7;

// Returns true if the given link is likely to be personalized to the user,
// which makes it unlikely that the URL works as intended when shared.
bool IsLinkLikelyPersonalized(const GURL& url) {
  // TODO: b/366237507 - Add more domains.
  static constexpr std::pair<std::string_view, std::string_view> kBlocklist[] =
      {
          {"mail.google.com", "/chat/"},
          {"mail.google.com", "/mail/"},
      };

  for (const auto& [domain, path_prefix] : kBlocklist) {
    if (url.DomainIs(domain) && base::StartsWith(url.path(), path_prefix)) {
      return true;
    }
  }
  return false;
}

}  // namespace

PickerLinkSuggester::PickerLinkSuggester(Profile* profile) {
  history_service_ = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  favicon_service_ = FaviconServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
}

PickerLinkSuggester::~PickerLinkSuggester() = default;

void PickerLinkSuggester::GetSuggestedLinks(size_t max_links,
                                            SuggestedLinksCallback callback) {
  CHECK(history_service_);
  history::QueryOptions options;
  options.max_count = max_links;
  options.SetRecentDayRange(kRecentDayRange);
  history_service_->QueryHistory(
      std::u16string(), options,
      base::BindOnce(&PickerLinkSuggester::OnGetBrowsingHistory,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      &history_query_tracker_);
}

void PickerLinkSuggester::OnGetBrowsingHistory(SuggestedLinksCallback callback,
                                               history::QueryResults results) {
  std::vector<history::URLResult> filtered_results;
  base::ranges::copy_if(
      results, std::back_inserter(filtered_results),
      [](const history::URLResult result) {
        if (base::FeatureList::IsEnabled(ash::features::kPickerFilterLinks) &&
            IsLinkLikelyPersonalized(result.url())) {
          return false;
        }
        return result.url().SchemeIsHTTPOrHTTPS();
      });

  if (favicon_service_) {
    favicon_query_trackers_ =
        std::vector<base::CancelableTaskTracker>(filtered_results.size());
    auto barrier_callback = base::BarrierCallback<ash::PickerSearchResult>(
        /*num_callbacks=*/filtered_results.size(),
        /*done_callback=*/std::move(callback));

    for (size_t i = 0; i < filtered_results.size(); ++i) {
      favicon_service_->GetFaviconImageForPageURL(
          filtered_results[i].url(),
          base::BindOnce(&PickerLinkSuggester::OnGetFaviconImage,
                         weak_factory_.GetWeakPtr(), filtered_results[i],
                         barrier_callback),
          &favicon_query_trackers_[i]);
    }
  } else {
    // Fallback to placeholder icon if favicon service is not available.
    std::vector<ash::PickerSearchResult> picker_search_results;
    for (const auto& result : filtered_results) {
      picker_search_results.push_back(ash::PickerBrowsingHistoryResult(
          result.url(), result.title(),
          ui::ImageModel::FromVectorIcon(ash::kOmniboxGenericIcon,
                                         cros_tokens::kCrosSysOnSurface),
          false));
    }
    std::move(callback).Run(picker_search_results);
  }
}

void PickerLinkSuggester::OnGetFaviconImage(
    history::URLResult result,
    base::OnceCallback<void(ash::PickerSearchResult)> callback,
    const favicon_base::FaviconImageResult& favicon_image_result) {
  std::move(callback).Run(ash::PickerBrowsingHistoryResult(
      result.url(), result.title(),
      favicon_image_result.image.IsEmpty()
          ? ui::ImageModel::FromVectorIcon(ash::kOmniboxGenericIcon,
                                           cros_tokens::kCrosSysOnSurface)
          : ui::ImageModel::FromImage(favicon_image_result.image),
      false));
}
