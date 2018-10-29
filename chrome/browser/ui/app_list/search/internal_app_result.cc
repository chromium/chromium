// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/internal_app_result.h"

#include <utility>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/favicon/large_icon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_context_menu.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/internal_app/internal_app_metadata.h"
#include "chrome/browser/ui/app_list/search/search_util.h"
#include "components/favicon/core/favicon_server_fetcher_params.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/fallback_icon_style.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace app_list {

namespace {

void RecordShowHistogram(InternalAppName name) {
  UMA_HISTOGRAM_ENUMERATION(
      "Apps.AppListSearchResultInternalApp.Show", name);
}

void RecordOpenHistogram(InternalAppName name) {
  UMA_HISTOGRAM_ENUMERATION(
      "Apps.AppListSearchResultInternalApp.Open", name);
}

}  // namespace

InternalAppResult::InternalAppResult(Profile* profile,
                                     const std::string& app_id,
                                     AppListControllerDelegate* controller,
                                     bool is_recommendation)
    : AppResult(profile, app_id, controller, is_recommendation),
      weak_factory_(this) {
  set_id(app_id);
  SetResultType(ResultType::kInternalApp);
  SetIcon(GetIconForResourceId(
      GetIconResourceIdByAppId(app_id),
      AppListConfig::instance().search_tile_icon_dimension()));
  if (display_type() == ash::SearchResultDisplayType::kRecommendation) {
    SetChipIcon(GetIconForResourceId(
        GetIconResourceIdByAppId(app_id),
        AppListConfig::instance().suggestion_chip_icon_dimension()));
  }

  if (id() == kInternalAppIdContinueReading) {
    large_icon_service_ =
        LargeIconServiceFactory::GetForBrowserContext(profile);
    UpdateContinueReadingFavicon(/*continue_to_google_server=*/true);
  }

  RecordShowHistogram(GetInternalAppNameByAppId(app_id));
}

InternalAppResult::~InternalAppResult() = default;

void InternalAppResult::ExecuteLaunchCommand(int event_flags) {
  Open(event_flags);
}

void InternalAppResult::Open(int event_flags) {
  // Record the search metric if the result is not a suggested app.
  if (display_type() != DisplayType::kRecommendation)
    RecordHistogram(APP_SEARCH_RESULT);

  RecordOpenHistogram(GetInternalAppNameByAppId(id()));

  if (id() == kInternalAppIdContinueReading &&
      url_for_continuous_reading_.is_valid()) {
    controller()->OpenURL(profile(), url_for_continuous_reading_,
                          ui::PAGE_TRANSITION_GENERATED,
                          ui::DispositionFromEventFlags(event_flags));
    return;
  }

  OpenInternalApp(id(), profile(), event_flags);
}

void InternalAppResult::UpdateContinueReadingFavicon(
    bool continue_to_google_server) {
  base::string16 title;
  GURL url;
  if (HasRecommendableForeignTab(profile(), &title, &url)) {
    url_for_continuous_reading_ = url;

    // Foreign tab could be updated since the title was set the last time.
    // Update the title every time.
    // TODO(wutao): If |title| is empty, use default title string.
    if (!title.empty())
      SetTitle(title);

    // Desired size of the icon. If not available, a smaller one will be used.
    constexpr int min_source_size_in_pixel = 16;
    constexpr int desired_size_in_pixel = 32;
    large_icon_service_->GetLargeIconImageOrFallbackStyle(
        url_for_continuous_reading_, min_source_size_in_pixel,
        desired_size_in_pixel,
        base::BindRepeating(&InternalAppResult::OnGetFaviconFromCacheFinished,
                            weak_factory_.GetWeakPtr(),
                            continue_to_google_server),
        &task_tracker_);
  }
}

void InternalAppResult::OnGetFaviconFromCacheFinished(
    bool continue_to_google_server,
    const favicon_base::LargeIconImageResult& image_result) {
  if (!image_result.image.IsEmpty()) {
    // Continue Reading app will only be shown in suggestion chip.
    SetChipIcon(*image_result.image.ToImageSkia());
    // Update the time when the icon was last requested to postpone the
    // automatic eviction of the favicon from the favicon database.
    large_icon_service_->TouchIconFromGoogleServer(image_result.icon_url);
    return;
  }

  if (!continue_to_google_server)
    return;

  // Try to fetch the favicon from a Google favicon server.
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("app_suggestion_get_favicon", R"(
        semantics {
          sender: "App Suggestion"
          description:
            "Sends a request to a Google server to retrieve the favicon bitmap "
            "for an article suggestion on the Launcher (URLs are public and "
            "provided by Google)."
          trigger:
            "A request can be sent if Chrome does not have a favicon for a "
            "particular page."
          data: "Page URL and desired icon size."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled by settings."
          policy_exception_justification: "Not implemented."
        })");
  large_icon_service_
      ->GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
          favicon::FaviconServerFetcherParams::CreateForDesktop(
              url_for_continuous_reading_),
          /*may_page_url_be_private=*/false, traffic_annotation,
          base::BindRepeating(
              &InternalAppResult::OnGetFaviconFromGoogleServerFinished,
              weak_factory_.GetWeakPtr()));
}

void InternalAppResult::OnGetFaviconFromGoogleServerFinished(
    favicon_base::GoogleFaviconServerRequestStatus status) {
  if (status != favicon_base::GoogleFaviconServerRequestStatus::SUCCESS)
    return;

  UpdateContinueReadingFavicon(/*continue_to_google_server=*/false);
}

void InternalAppResult::GetContextMenuModel(GetMenuModelCallback callback) {
  const auto* internal_app = app_list::FindInternalApp(id());
  DCHECK(internal_app);
  if (!internal_app->show_in_launcher) {
    std::move(callback).Run(nullptr);
    return;
  }

  if (!context_menu_) {
    context_menu_ = std::make_unique<AppContextMenu>(nullptr, profile(), id(),
                                                     controller());
  }
  context_menu_->GetMenuModel(std::move(callback));
}

AppContextMenu* InternalAppResult::GetAppContextMenu() {
  return context_menu_.get();
}

}  // namespace app_list
