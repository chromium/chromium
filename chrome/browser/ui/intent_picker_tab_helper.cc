// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/intent_picker_tab_helper.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "chrome/browser/apps/intent_helper/intent_picker_auto_display_prefs.h"
#include "chrome/browser/apps/intent_helper/intent_picker_features.h"
#include "chrome/browser/apps/intent_helper/intent_picker_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "content/public/browser/navigation_handle.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/intent_helper/metrics/intent_handling_metrics.h"
#endif

namespace {

apps::mojom::AppType GetAppType(apps::PickerEntryType picker_entry_type) {
  apps::mojom::AppType app_type = apps::mojom::AppType::kUnknown;
  switch (picker_entry_type) {
    case apps::PickerEntryType::kUnknown:
    case apps::PickerEntryType::kDevice:
      break;
    case apps::PickerEntryType::kArc:
      app_type = apps::mojom::AppType::kArc;
      break;
    case apps::PickerEntryType::kWeb:
      app_type = apps::mojom::AppType::kWeb;
      break;
    case apps::PickerEntryType::kMacOs:
      app_type = apps::mojom::AppType::kMacOs;
      break;
  }
  return app_type;
}

web_app::WebAppRegistrar* MaybeGetWebAppRegistrar(
    content::WebContents* web_contents) {
  // Profile for web contents might not contain a web app provider. eg. kiosk
  // profile in Chrome OS.
  auto* provider = web_app::WebAppProvider::GetForWebContents(web_contents);
  return provider ? &provider->registrar() : nullptr;
}

web_app::WebAppInstallManager* MaybeGetWebAppInstallManager(
    content::WebContents* web_contents) {
  // Profile for web contents might not contain a web app provider. eg. kiosk
  // profile in Chrome OS.
  auto* provider = web_app::WebAppProvider::GetForWebContents(web_contents);
  return provider ? &provider->install_manager() : nullptr;
}

void LoadSingleAppIcon(Profile* profile,
                       apps::mojom::AppType app_type,
                       const std::string& app_id,
                       int size_in_dip,
                       base::OnceCallback<void(apps::IconValuePtr)> callback) {
  constexpr bool allow_placeholder_icon = false;
  if (base::FeatureList::IsEnabled(features::kAppServiceLoadIconWithoutMojom)) {
    apps::AppServiceProxyFactory::GetForProfile(profile)->LoadIcon(
        apps::ConvertMojomAppTypToAppType(app_type), app_id,
        apps::IconType::kStandard, size_in_dip, allow_placeholder_icon,
        std::move(callback));
  } else {
    apps::AppServiceProxyFactory::GetForProfile(profile)->LoadIcon(
        app_type, app_id, apps::mojom::IconType::kStandard, size_in_dip,
        allow_placeholder_icon,
        apps::MojomIconValueToIconValueCallback(std::move(callback)));
  }
}

}  // namespace

IntentPickerTabHelper::~IntentPickerTabHelper() = default;

// static
void IntentPickerTabHelper::ShowOrHideIcon(content::WebContents* web_contents,
                                           bool should_show_icon) {
  IntentPickerTabHelper* tab_helper = FromWebContents(web_contents);
  if (!tab_helper)
    return;

  if (apps::features::LinkCapturingUiUpdateEnabled()) {
    tab_helper->app_icon_ = ui::ImageModel();
    tab_helper->should_show_collapsed_chip_ = false;
    tab_helper->last_shown_app_id_ = std::string();
    tab_helper->last_shown_origin_ = url::Origin();
  }

  tab_helper->ShowOrHideIconInternal(should_show_icon);
}

void IntentPickerTabHelper::ShowIconForApps(
    const std::vector<apps::IntentPickerAppInfo>& apps) {
  if (apps::features::AppIconInIntentChipEnabled()) {
    if (apps.size() == 1 && apps[0].launch_name != last_shown_app_id_) {
      const std::string& app_id = apps[0].launch_name;
      auto app_type = GetAppType(apps[0].type);

      Profile* profile =
          Profile::FromBrowserContext(web_contents()->GetBrowserContext());

      last_shown_app_id_ = app_id;
      LoadSingleAppIcon(
          profile, app_type, app_id, GetLayoutConstant(LOCATION_BAR_ICON_SIZE),
          base::BindOnce(&IntentPickerTabHelper::OnAppIconLoadedForChip,
                         weak_factory_.GetWeakPtr(), app_id));
      return;
    } else if (apps.size() != 1) {
      app_icon_ = ui::ImageModel();
      last_shown_app_id_ = std::string();
    }
  }

  ShowIconForLinkIntent(!apps.empty());
}

IntentPickerTabHelper::IntentPickerTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<IntentPickerTabHelper>(*web_contents),
      registrar_(MaybeGetWebAppRegistrar(web_contents)),
      install_manager_(MaybeGetWebAppInstallManager(web_contents)) {
  if (install_manager_)
    install_manager_observation_.Observe(install_manager_.get());
}

// static
void IntentPickerTabHelper::LoadAppIcons(
    content::WebContents* web_contents,
    std::vector<apps::IntentPickerAppInfo> apps,
    IntentPickerIconLoaderCallback callback) {
  IntentPickerTabHelper* tab_helper = FromWebContents(web_contents);
  if (!tab_helper) {
    std::move(callback).Run(std::move(apps));
    return;
  }
  tab_helper->LoadAppIcon(std::move(apps), std::move(callback), 0);
}

void IntentPickerTabHelper::SetIconUpdateCallbackForTesting(
    base::OnceClosure callback) {
  icon_update_closure_ = std::move(callback);
}

void IntentPickerTabHelper::OnAppIconLoaded(
    std::vector<apps::IntentPickerAppInfo> apps,
    IntentPickerIconLoaderCallback callback,
    size_t index,
    apps::IconValuePtr icon_value) {
  gfx::Image image =
      (icon_value && icon_value->icon_type == apps::IconType::kStandard)
          ? gfx::Image(icon_value->uncompressed)
          : gfx::Image();
  apps[index].icon_model = ui::ImageModel::FromImage(image);

  if (index == apps.size() - 1)
    std::move(callback).Run(std::move(apps));
  else
    LoadAppIcon(std::move(apps), std::move(callback), index + 1);
}

void IntentPickerTabHelper::LoadAppIcon(
    std::vector<apps::IntentPickerAppInfo> apps,
    IntentPickerIconLoaderCallback callback,
    size_t index) {
  if (index >= apps.size()) {
    std::move(callback).Run(std::move(apps));
    return;
  }

  const std::string& app_id = apps[index].launch_name;
  auto app_type = GetAppType(apps[index].type);

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());

  LoadSingleAppIcon(profile, app_type, app_id, gfx::kFaviconSize,
                    base::BindOnce(&IntentPickerTabHelper::OnAppIconLoaded,
                                   weak_factory_.GetWeakPtr(), std::move(apps),
                                   std::move(callback), index));
}

void IntentPickerTabHelper::UpdateCollapsedState(bool should_show_icon) {
  GURL url = web_contents()->GetLastCommittedURL();

  if (!should_show_icon || url.is_empty()) {
    should_show_collapsed_chip_ = false;
    last_shown_origin_ = url::Origin();
    return;
  }

  url::Origin origin = url::Origin::Create(url);

  // Determine whether to show the Chip as expanded/collapsed whenever the
  // origin changes.
  if (!origin.IsSameOriginWith(last_shown_origin_)) {
    last_shown_origin_ = origin;
    Profile* profile =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    auto chip_state =
        IntentPickerAutoDisplayPrefs ::GetChipStateAndIncrementCounter(profile,
                                                                       url);
    should_show_collapsed_chip_ =
        chip_state == IntentPickerAutoDisplayPrefs::ChipState::kCollapsed;
  }
}

void IntentPickerTabHelper::OnAppIconLoadedForChip(const std::string& app_id,
                                                   apps::IconValuePtr icon) {
  if (app_id != last_shown_app_id_)
    return;

  if (icon && icon->icon_type == apps::IconType::kStandard) {
    app_icon_ = ui::ImageModel::FromImage(gfx::Image(icon->uncompressed));
  } else {
    last_shown_app_id_ = std::string();
    app_icon_ = ui::ImageModel();
  }

  ShowIconForLinkIntent(true);
}

void IntentPickerTabHelper::ShowIconForLinkIntent(bool should_show_icon) {
#if BUILDFLAG(IS_CHROMEOS)
  if (should_show_icon && !should_show_icon_) {
    // This point doesn't exactly match when the icon is shown in the UI (e.g.
    // if the tab is not active), but recording here corresponds more closely to
    // navigations which cause the icon to appear.
    apps::IntentHandlingMetrics::RecordIntentPickerIconEvent(
        apps::IntentHandlingMetrics::IntentPickerIconEvent::kIconShown);
  }
#endif

  if (apps::features::LinkCapturingUiUpdateEnabled()) {
    UpdateCollapsedState(should_show_icon);
  }

  ShowOrHideIconInternal(should_show_icon);
}

void IntentPickerTabHelper::ShowOrHideIconInternal(bool should_show_icon) {
  should_show_icon_ = should_show_icon;

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (!browser)
    return;
  browser->window()->UpdatePageActionIcon(PageActionIconType::kIntentPicker);

  if (icon_update_closure_)
    std::move(icon_update_closure_).Run();
}

void IntentPickerTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // For a http/https scheme URL navigation, we will check if the
  // url can be handled by some apps, and show intent picker icon
  // or bubble if there are some apps available. We only want to check this if
  // the navigation happens in the primary main frame, and the navigation is not
  // the same document with same URL.
  if (!web_contents()) {
    return;
  }
  if (navigation_handle->IsInPrimaryMainFrame() &&
      navigation_handle->HasCommitted() &&
      (!navigation_handle->IsSameDocument() ||
       navigation_handle->GetURL() !=
           navigation_handle->GetPreviousMainFrameURL())) {
    bool is_valid_page = navigation_handle->GetURL().SchemeIsHTTPOrHTTPS() &&
                         !navigation_handle->IsErrorPage();
    if (is_valid_page) {
      apps::MaybeShowIntentPicker(navigation_handle);
    } else {
      ShowOrHideIcon(web_contents(), /*should_show_icon=*/false);
    }
  }
}

void IntentPickerTabHelper::OnWebAppWillBeUninstalled(
    const web_app::AppId& app_id) {
  // WebAppTabHelper has an app_id but it is reset during
  // OnWebAppWillBeUninstalled so using FindAppWithUrlInScope.
  absl::optional<web_app::AppId> local_app_id =
      registrar_->FindAppWithUrlInScope(web_contents()->GetLastCommittedURL());
  if (app_id == local_app_id)
    ShowOrHideIcon(web_contents(), /*should_show_icon=*/false);
}

void IntentPickerTabHelper::OnWebAppInstallManagerDestroyed() {
  install_manager_observation_.Reset();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(IntentPickerTabHelper);
