// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/app_service_app_result.h"

#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "base/bind.h"
#include "chrome/browser/apps/app_service/app_service_metrics.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/release_notes/release_notes_storage.h"
#include "chrome/browser/favicon/large_icon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/app_list/app_service/app_service_app_item.h"
#include "chrome/browser/ui/app_list/internal_app/internal_app_metadata.h"
#include "components/favicon/core/large_icon_service.h"
#include "extensions/common/extension.h"

namespace app_list {

AppServiceAppResult::AppServiceAppResult(Profile* profile,
                                         const std::string& app_id,
                                         AppListControllerDelegate* controller,
                                         bool is_recommendation,
                                         apps::IconLoader* icon_loader)
    : AppResult(profile, app_id, controller, is_recommendation),
      icon_loader_(icon_loader),
      app_type_(apps::mojom::AppType::kUnknown),
      is_platform_app_(false),
      show_in_launcher_(false) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);

  if (proxy) {
    proxy->AppRegistryCache().ForOneApp(
        app_id, [this](const apps::AppUpdate& update) {
          app_type_ = update.AppType();
          is_platform_app_ =
              update.IsPlatformApp() == apps::mojom::OptionalBool::kTrue;
          show_in_launcher_ =
              update.ShowInLauncher() == apps::mojom::OptionalBool::kTrue;
        });

    constexpr bool allow_placeholder_icon = true;
    CallLoadIcon(false, allow_placeholder_icon);
    if (display_type() == ash::SearchResultDisplayType::kRecommendation) {
      CallLoadIcon(true, allow_placeholder_icon);
    }
  }

  switch (app_type_) {
    case apps::mojom::AppType::kBuiltIn:
      set_id(app_id);
      // TODO(crbug.com/826982): Is this SetResultType call necessary?? Does
      // anyone care about the kInternalApp vs kInstalledApp distinction?
      SetResultType(ResultType::kInternalApp);
      apps::RecordBuiltInAppSearchResult(app_id);
      break;
    case apps::mojom::AppType::kExtension:
      // TODO(crbug.com/826982): why do we pass the URL and not the app_id??
      // Can we replace this by the simpler "set_id(app_id)", and therefore
      // pull that out of the switch?
      set_id(extensions::Extension::GetBaseURLFromExtensionId(app_id).spec());
      break;
    default:
      set_id(app_id);
      break;
  }

  if (IsSuggestionChip(id()))
    HandleSuggestionChip(profile);
}

AppServiceAppResult::~AppServiceAppResult() = default;

void AppServiceAppResult::Open(int event_flags) {
  Launch(event_flags,
         (display_type() == ash::SearchResultDisplayType::kRecommendation)
             ? apps::mojom::LaunchSource::kFromAppListRecommendation
             : apps::mojom::LaunchSource::kFromAppListQuery);
}

void AppServiceAppResult::GetContextMenuModel(GetMenuModelCallback callback) {
  // TODO(crbug.com/826982): drop the (app_type_ == etc), and check
  // show_in_launcher_ for all app types?
  if ((app_type_ == apps::mojom::AppType::kBuiltIn) && !show_in_launcher_) {
    std::move(callback).Run(nullptr);
    return;
  }

  context_menu_ = AppServiceAppItem::MakeAppContextMenu(
      app_type_, this, profile(), app_id(), controller(), is_platform_app_);
  context_menu_->GetMenuModel(std::move(callback));
}

void AppServiceAppResult::OnVisibilityChanged(bool visibility) {
  if (id() == ash::kReleaseNotesAppId && visibility) {
    DCHECK(chromeos::ReleaseNotesStorage(profile()).ShouldShowSuggestionChip());
    chromeos::ReleaseNotesStorage(profile())
        .DecreaseTimesLeftToShowSuggestionChip();
  }
}

ash::SearchResultType AppServiceAppResult::GetSearchResultType() const {
  switch (app_type_) {
    case apps::mojom::AppType::kArc:
      return ash::PLAY_STORE_APP;
    case apps::mojom::AppType::kBuiltIn:
      return ash::INTERNAL_APP;
    case apps::mojom::AppType::kCrostini:
      return ash::CROSTINI_APP;
    case apps::mojom::AppType::kExtension:
    case apps::mojom::AppType::kWeb:
      return ash::EXTENSION_APP;
    default:
      NOTREACHED();
      return ash::SEARCH_RESULT_TYPE_BOUNDARY;
  }
}

AppContextMenu* AppServiceAppResult::GetAppContextMenu() {
  return context_menu_.get();
}

void AppServiceAppResult::ExecuteLaunchCommand(int event_flags) {
  Launch(event_flags, apps::mojom::LaunchSource::kFromAppListQueryContextMenu);
}

void AppServiceAppResult::Launch(int event_flags,
                                 apps::mojom::LaunchSource launch_source) {
  if (id() == ash::kInternalAppIdContinueReading &&
      url_for_continuous_reading_.is_valid()) {
    apps::RecordAppLaunch(id(), launch_source);
    controller()->OpenURL(profile(), url_for_continuous_reading_,
                          ui::PAGE_TRANSITION_GENERATED,
                          ui::DispositionFromEventFlags(event_flags));
    return;
  }

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());
  if (proxy) {
    proxy->Launch(app_id(), event_flags, launch_source,
                  controller()->GetAppListDisplayId());
  }
}

void AppServiceAppResult::CallLoadIcon(bool chip, bool allow_placeholder_icon) {
  if (icon_loader_) {
    // If |icon_loader_releaser_| is non-null, assigning to it will signal to
    // |icon_loader_| that the previous icon is no longer being used, as a hint
    // that it could be flushed from any caches.
    icon_loader_releaser_ = icon_loader_->LoadIcon(
        app_type_, app_id(), apps::mojom::IconCompression::kUncompressed,
        chip ? ash::AppListConfig::instance().suggestion_chip_icon_dimension()
             : ash::AppListConfig::instance().GetPreferredIconDimension(
                   display_type()),
        allow_placeholder_icon,
        base::BindOnce(&AppServiceAppResult::OnLoadIcon,
                       weak_ptr_factory_.GetWeakPtr(), chip));
  }
}

void AppServiceAppResult::OnLoadIcon(bool chip,
                                     apps::mojom::IconValuePtr icon_value) {
  if (icon_value->icon_compression !=
      apps::mojom::IconCompression::kUncompressed) {
    return;
  }

  if (chip) {
    SetChipIcon(icon_value->uncompressed);
  } else {
    SetIcon(icon_value->uncompressed);
  }

  if (icon_value->is_placeholder_icon) {
    constexpr bool allow_placeholder_icon = false;
    CallLoadIcon(chip, allow_placeholder_icon);
  }
}

void AppServiceAppResult::HandleSuggestionChip(Profile* profile) {
  if (id() == ash::kInternalAppIdContinueReading) {
    large_icon_service_ =
        LargeIconServiceFactory::GetForBrowserContext(profile);
    UpdateContinueReadingFavicon(/*continue_to_google_server=*/true);
  }

  // Set these values to make sure that the chip will show up
  // in the proper position.
  SetDisplayIndex(ash::SearchResultDisplayIndex::kFirstIndex);
  SetDisplayLocation(
      ash::SearchResultDisplayLocation::kSuggestionChipContainer);

  if (id() == ash::kReleaseNotesAppId) {
    SetNotifyVisibilityChange(true);
    // Make sure that if both Continue Reading and Release Notes are available,
    // Release Notes shows up first in the suggestion chip container.
    SetPositionPriority(1.0f);
  }
}

void AppServiceAppResult::UpdateContinueReadingFavicon(
    bool continue_to_google_server) {
  base::string16 title;
  GURL url;
  if (app_list::HasRecommendableForeignTab(profile(), &title, &url,
                                           /*test_delegate=*/nullptr)) {
    url_for_continuous_reading_ = url;

    // Foreign tab could be updated since the title was set the last time.
    // Update the title every time.
    // TODO(wutao): If |title| is empty, use default title string.
    if (!title.empty())
      SetTitle(title);

    // Desired size of the icon. If not available, a smaller one will be used.
    constexpr int min_source_size_in_pixel = 16;
    constexpr int desired_size_in_pixel = 32;
    large_icon_service_->GetLargeIconImageOrFallbackStyleForPageUrl(
        url_for_continuous_reading_, min_source_size_in_pixel,
        desired_size_in_pixel,
        base::BindRepeating(&AppServiceAppResult::OnGetFaviconFromCacheFinished,
                            weak_ptr_factory_.GetWeakPtr(),
                            continue_to_google_server),
        &task_tracker_);
  }
}

void AppServiceAppResult::OnGetFaviconFromCacheFinished(
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
          url_for_continuous_reading_,
          /*may_page_url_be_private=*/false,
          /*should_trim_page_url_path=*/false, traffic_annotation,
          base::BindRepeating(
              &AppServiceAppResult::OnGetFaviconFromGoogleServerFinished,
              weak_ptr_factory_.GetWeakPtr()));
}

void AppServiceAppResult::OnGetFaviconFromGoogleServerFinished(
    favicon_base::GoogleFaviconServerRequestStatus status) {
  if (status != favicon_base::GoogleFaviconServerRequestStatus::SUCCESS)
    return;

  UpdateContinueReadingFavicon(/*continue_to_google_server=*/false);
}

}  // namespace app_list
