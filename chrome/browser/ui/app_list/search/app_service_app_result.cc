// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/app_service_app_result.h"

#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_service_metrics.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/favicon/large_icon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/app_list/app_service/app_service_app_item.h"
#include "chrome/browser/ui/app_list/app_service/app_service_context_menu.h"
#include "chrome/browser/ui/app_list/internal_app/internal_app_metadata.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/common/chrome_features.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"

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
  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->AppRegistryCache()
      .ForOneApp(app_id, [this](const apps::AppUpdate& update) {
        app_type_ = update.AppType();
        is_platform_app_ =
            update.IsPlatformApp() == apps::mojom::OptionalBool::kTrue;
        show_in_launcher_ =
            update.ShowInLauncher() == apps::mojom::OptionalBool::kTrue;

        if (update.Readiness() == apps::mojom::Readiness::kDisabledByPolicy) {
          SetAccessibleName(l10n_util::GetStringFUTF16(
              IDS_APP_ACCESSIBILITY_BLOCKED_INSTALLED_APP_ANNOUNCEMENT,
              base::UTF8ToUTF16(update.ShortName())));
        } else if (update.Paused() == apps::mojom::OptionalBool::kTrue) {
          SetAccessibleName(l10n_util::GetStringFUTF16(
              IDS_APP_ACCESSIBILITY_PAUSED_INSTALLED_APP_ANNOUNCEMENT,
              base::UTF8ToUTF16(update.ShortName())));
        }
      });

  constexpr bool allow_placeholder_icon = true;
  CallLoadIcon(false, allow_placeholder_icon);
  if (is_recommendation) {
    CallLoadIcon(true, allow_placeholder_icon);
  }

  SetMetricsType(GetSearchResultType());

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
         (is_recommendation()
              ? apps::mojom::LaunchSource::kFromAppListRecommendation
              : apps::mojom::LaunchSource::kFromAppListQuery));
}

void AppServiceAppResult::GetContextMenuModel(GetMenuModelCallback callback) {
  // TODO(crbug.com/826982): drop the (app_type_ == etc), and check
  // show_in_launcher_ for all app types?
  if ((app_type_ == apps::mojom::AppType::kBuiltIn) && !show_in_launcher_) {
    std::move(callback).Run(nullptr);
    return;
  }

  context_menu_ = std::make_unique<AppServiceContextMenu>(
      this, profile(), app_id(), controller());
  context_menu_->GetMenuModel(std::move(callback));
}

ash::SearchResultType AppServiceAppResult::GetSearchResultType() const {
  switch (app_type_) {
    case apps::mojom::AppType::kArc:
      return ash::PLAY_STORE_APP;
    case apps::mojom::AppType::kBuiltIn:
      return ash::INTERNAL_APP;
    case apps::mojom::AppType::kPluginVm:
      return ash::PLUGIN_VM_APP;
    case apps::mojom::AppType::kCrostini:
      return ash::CROSTINI_APP;
    case apps::mojom::AppType::kExtension:
    case apps::mojom::AppType::kWeb:
    case apps::mojom::AppType::kSystemWeb:
      return ash::EXTENSION_APP;
    case apps::mojom::AppType::kLacros:
      return ash::LACROS;
    case apps::mojom::AppType::kRemote:
      return ash::REMOTE_APP;
    case apps::mojom::AppType::kBorealis:
      return ash::BOREALIS_APP;
    case apps::mojom::AppType::kMacOs:
    case apps::mojom::AppType::kUnknown:
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

  apps::AppServiceProxyChromeOs* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());

  // For Chrome apps or Web apps, if it is non-platform app, it could be
  // selecting an existing delegate for the app, so call
  // ChromeLauncherController's ActivateApp interface. Platform apps or ARC
  // apps, Crostini apps treat activations as a launch. The app can decide
  // whether to show a new window or focus an existing window as it sees fit.
  //
  // TODO(crbug.com/1026730): Move this special case to ExtensionApps,
  // when AppService Instance feature is done.
  bool is_active_app = false;
  proxy->AppRegistryCache().ForOneApp(
      app_id(), [&is_active_app](const apps::AppUpdate& update) {
        if (update.AppType() == apps::mojom::AppType::kCrostini ||
            ((update.AppType() == apps::mojom::AppType::kExtension ||
              update.AppType() == apps::mojom::AppType::kWeb) &&
             update.IsPlatformApp() == apps::mojom::OptionalBool::kFalse)) {
          is_active_app = true;
        }
      });
  if (is_active_app) {
    ChromeLauncherController::instance()->ActivateApp(
        app_id(), ash::LAUNCH_FROM_APP_LIST_SEARCH, event_flags,
        controller()->GetAppListDisplayId());
  } else {
    proxy->Launch(app_id(), event_flags, launch_source,
                  apps::MakeWindowInfo(controller()->GetAppListDisplayId()));
  }
}

void AppServiceAppResult::CallLoadIcon(bool chip, bool allow_placeholder_icon) {
  if (icon_loader_) {
    // If |icon_loader_releaser_| is non-null, assigning to it will signal to
    // |icon_loader_| that the previous icon is no longer being used, as a hint
    // that it could be flushed from any caches.
    auto icon_type =
        (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon))
            ? apps::mojom::IconType::kStandard
            : apps::mojom::IconType::kUncompressed;
    icon_loader_releaser_ = icon_loader_->LoadIcon(
        app_type_, app_id(), icon_type,
        chip ? ash::SharedAppListConfig::instance()
                   .suggestion_chip_icon_dimension()
             : ash::SharedAppListConfig::instance().GetPreferredIconDimension(
                   display_type()),
        allow_placeholder_icon,
        base::BindOnce(&AppServiceAppResult::OnLoadIcon,
                       weak_ptr_factory_.GetWeakPtr(), chip));
  }
}

void AppServiceAppResult::OnLoadIcon(bool chip,
                                     apps::mojom::IconValuePtr icon_value) {
  auto icon_type =
      (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon))
          ? apps::mojom::IconType::kStandard
          : apps::mojom::IconType::kUncompressed;
  if (icon_value->icon_type != icon_type) {
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
  SetDisplayType(ash::SearchResultDisplayType::kChip);
}

void AppServiceAppResult::UpdateContinueReadingFavicon(
    bool continue_to_google_server) {
  std::u16string title;
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
        base::BindOnce(&AppServiceAppResult::OnGetFaviconFromCacheFinished,
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
          base::BindOnce(
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
