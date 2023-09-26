// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/intent_picker_tab_helper.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "chrome/browser/apps/intent_helper/intent_chip_display_prefs.h"
#include "chrome/browser/apps/intent_helper/intent_picker_features.h"
#include "chrome/browser/apps/intent_helper/intent_picker_helpers.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/chrome_no_state_prefetch_contents_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "content/public/browser/navigation_handle.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/intent_helper/chromeos_intent_picker_helpers.h"
#include "chrome/browser/apps/intent_helper/metrics/intent_handling_metrics.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {

apps::AppType GetAppType(apps::PickerEntryType picker_entry_type) {
  apps::AppType app_type = apps::AppType::kUnknown;
  switch (picker_entry_type) {
    case apps::PickerEntryType::kUnknown:
    case apps::PickerEntryType::kDevice:
      break;
    case apps::PickerEntryType::kArc:
      app_type = apps::AppType::kArc;
      break;
    case apps::PickerEntryType::kWeb:
      app_type = apps::AppType::kWeb;
      break;
    case apps::PickerEntryType::kMacOs:
      app_type = apps::AppType::kMacOs;
      break;
  }
  return app_type;
}

web_app::WebAppRegistrar* MaybeGetWebAppRegistrar(
    content::WebContents* web_contents) {
  // Profile for web contents might not contain a web app provider. eg. kiosk
  // profile in Chrome OS.
  auto* provider = web_app::WebAppProvider::GetForWebContents(web_contents);
  return provider ? &provider->registrar_unsafe() : nullptr;
}

web_app::WebAppInstallManager* MaybeGetWebAppInstallManager(
    content::WebContents* web_contents) {
  // Profile for web contents might not contain a web app provider. eg. kiosk
  // profile in Chrome OS.
  auto* provider = web_app::WebAppProvider::GetForWebContents(web_contents);
  return provider ? &provider->install_manager() : nullptr;
}

void LoadSingleAppIcon(Profile* profile,
                       apps::AppType app_type,
                       const std::string& app_id,
                       int size_in_dip,
                       base::OnceCallback<void(apps::IconValuePtr)> callback) {
  apps::AppServiceProxyFactory::GetForProfile(profile)->LoadIcon(
      app_type, app_id, apps::IconType::kStandard, size_in_dip,
      /*allow_placeholder_icon=*/false, std::move(callback));
}

bool IsNavigatingToNewSite(content::NavigationHandle* navigation_handle) {
  return navigation_handle->IsInPrimaryMainFrame() &&
         navigation_handle->HasCommitted() &&
         (!navigation_handle->IsSameDocument() ||
          navigation_handle->GetURL() !=
              navigation_handle->GetPreviousPrimaryMainFrameURL());
}

bool ShouldConsiderWebContentsForIntentPicker(
    content::WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  bool is_prerender =
      prerender::ChromeNoStatePrefetchContentsDelegate::FromWebContents(
          web_contents) != nullptr;
  if (is_prerender || !web_app::AreWebAppsUserInstallable(profile)) {
    return false;
  }

  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    return false;
  }

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (browser && (browser->is_type_app() || browser->is_type_app_popup())) {
    return false;
  }
  return true;
}

void ShowIntentPickerBubbleForApps(
    content::WebContents* web_contents,
    bool show_stay_in_chrome,
    bool show_remember_selection,
    IntentPickerResponse callback,
    std::vector<apps::IntentPickerAppInfo> apps) {
  if (apps.empty()) {
    return;
  }

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser) {
    return;
  }

  browser->window()->ShowIntentPickerBubble(
      std::move(apps), show_stay_in_chrome, show_remember_selection,
      apps::IntentPickerBubbleType::kLinkCapturing, absl::nullopt,
      std::move(callback));
}

}  // namespace

IntentPickerTabHelper::~IntentPickerTabHelper() = default;

// static
void IntentPickerTabHelper::MaybeShowIntentPickerIcon(
    content::WebContents* web_contents) {
  CHECK(web_contents);
  IntentPickerTabHelper* helper =
      IntentPickerTabHelper::FromWebContents(web_contents);
  if (!ShouldConsiderWebContentsForIntentPicker(web_contents)) {
    helper->MaybeShowIconForApps({});
    return;
  }

  FindAllAppsForUrl(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()),
      web_contents->GetLastCommittedURL(),
      base::BindOnce(&IntentPickerTabHelper::MaybeShowIconForApps,
                     helper->per_navigation_weak_factory_.GetWeakPtr()));
}

// static
void IntentPickerTabHelper::ShowIntentPickerBubbleOrLaunchApp(
    content::WebContents* web_contents,
    const GURL& url) {
  CHECK(web_contents);
  IntentPickerTabHelper* helper =
      IntentPickerTabHelper::FromWebContents(web_contents);
  if (!ShouldConsiderWebContentsForIntentPicker(web_contents)) {
    return;
  }
  FindAllAppsForUrl(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()), url,
      base::BindOnce(&IntentPickerTabHelper::ShowIntentPickerOrLaunchAppImpl,
                     helper->per_navigation_weak_factory_.GetWeakPtr(), url));
}

// static
void IntentPickerTabHelper::ShowOrHideIcon(content::WebContents* web_contents,
                                           bool should_show_icon) {
  IntentPickerTabHelper* tab_helper = FromWebContents(web_contents);
  if (!tab_helper)
    return;

  if (apps::features::LinkCapturingUiUpdateEnabled()) {
    tab_helper->current_app_icon_ = ui::ImageModel();
    tab_helper->show_expanded_chip_from_usage_ = false;
    tab_helper->current_app_id_ = std::string();
    tab_helper->current_app_is_preferred_ = false;
    tab_helper->last_shown_origin_ = url::Origin();
  }

  tab_helper->ShowOrHideIconInternal(should_show_icon);
}

void IntentPickerTabHelper::MaybeShowIconForApps(
    std::vector<apps::IntentPickerAppInfo> apps) {
#if BUILDFLAG(IS_CHROMEOS)
  // We enter this block when we have apps available and there weren't any
  // previously.
  if (!should_show_icon_ && !apps.empty()) {
    // This point doesn't exactly match when the icon is shown in the UI (e.g.
    // if the tab is not active), but recording here corresponds more closely to
    // navigations which cause the icon to appear.
    apps::IntentHandlingMetrics::RecordIntentPickerIconEvent(
        apps::IntentHandlingMetrics::IntentPickerIconEvent::kIconShown);

    apps::IntentHandlingMetrics::RecordLinkCapturingEntryPointShown(apps);
  }
#endif

  if (apps::features::LinkCapturingUiUpdateEnabled()) {
    if (apps.size() == 1 && apps[0].launch_name != current_app_id_) {
      current_app_id_ = apps[0].launch_name;

      Profile* profile =
          Profile::FromBrowserContext(web_contents()->GetBrowserContext());

      // If this app is the preferred app to handle this URL, the icon will
      // always be shown as expanded, regardless of the usage-based decision
      // calculated in UpdateExpandedState().
      current_app_is_preferred_ =
          apps::AppServiceProxyFactory::GetForProfile(profile)
              ->PreferredAppsList()
              .IsPreferredAppForSupportedLinks(current_app_id_);

      LoadSingleAppIcon(
          profile, GetAppType(apps[0].type), current_app_id_,
          GetLayoutConstant(LOCATION_BAR_ICON_SIZE),
          base::BindOnce(&IntentPickerTabHelper::OnAppIconLoadedForChip,
                         per_navigation_weak_factory_.GetWeakPtr(),
                         current_app_id_));
      return;
    } else if (apps.size() != 1) {
      current_app_icon_ = ui::ImageModel();
      current_app_id_ = std::string();
      current_app_is_preferred_ = false;
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
    LoadAppIcon(std::move(apps), index + 1, std::move(callback));
}

void IntentPickerTabHelper::LoadAppIcon(
    std::vector<apps::IntentPickerAppInfo> apps,
    size_t index,
    IntentPickerIconLoaderCallback callback) {
  if (index >= apps.size()) {
    std::move(callback).Run(std::move(apps));
    return;
  }

  const std::string& app_id = apps[index].launch_name;
  auto app_type = GetAppType(apps[index].type);

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());

  LoadSingleAppIcon(
      profile, app_type, app_id, apps::GetIntentPickerBubbleIconSize(),
      base::BindOnce(&IntentPickerTabHelper::OnAppIconLoaded,
                     per_navigation_weak_factory_.GetWeakPtr(), std::move(apps),
                     std::move(callback), index));
}

void IntentPickerTabHelper::UpdateExpandedState(bool should_show_icon) {
  GURL url = web_contents()->GetLastCommittedURL();

  if (!should_show_icon || url.is_empty()) {
    show_expanded_chip_from_usage_ = false;
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
        IntentChipDisplayPrefs::GetChipStateAndIncrementCounter(profile, url);
    show_expanded_chip_from_usage_ =
        chip_state == IntentChipDisplayPrefs::ChipState::kExpanded;
  }
}

void IntentPickerTabHelper::OnAppIconLoadedForChip(const std::string& app_id,
                                                   apps::IconValuePtr icon) {
  if (app_id != current_app_id_)
    return;

  if (icon && icon->icon_type == apps::IconType::kStandard) {
    current_app_icon_ =
        ui::ImageModel::FromImage(gfx::Image(icon->uncompressed));
  } else {
    current_app_id_ = std::string();
    current_app_icon_ = ui::ImageModel();
  }

  ShowIconForLinkIntent(true);
}

void IntentPickerTabHelper::ShowIconForLinkIntent(bool should_show_icon) {
  if (apps::features::LinkCapturingUiUpdateEnabled()) {
    UpdateExpandedState(should_show_icon);
  }

  ShowOrHideIconInternal(should_show_icon);
}

void IntentPickerTabHelper::ShowOrHideIconInternal(bool should_show_icon) {
  should_show_icon_ = should_show_icon;

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (!browser)
    return;
  browser->window()->UpdatePageActionIcon(PageActionIconType::kIntentPicker);

  icon_resolved_after_last_navigation_ = true;
  if (icon_update_closure_)
    std::move(icon_update_closure_).Run();
}

void IntentPickerTabHelper::ShowIntentPickerOrLaunchAppImpl(
    const GURL& url,
    std::vector<apps::IntentPickerAppInfo> apps) {
  if (apps.empty()) {
    return;
  }
  if (web_contents()->IsBeingDestroyed()) {
    return;
  }

#if BUILDFLAG(IS_CHROMEOS)
  apps::IntentHandlingMetrics::RecordIntentPickerIconEvent(
      apps::IntentHandlingMetrics::IntentPickerIconEvent::kIconClicked);
#endif

  if (apps.size() == 1) {
    // If there is only a single available app, immediately launch it if either:
    // - LinkCapturingInfoBarEnabled() is enabled, or
    // - LinkCapturingUiUpdateEnabled() is enabled and the app is preferred for
    // this link.
    Profile* profile =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);

    bool should_launch_for_preferred_app =
        apps::features::LinkCapturingUiUpdateEnabled() &&
        proxy->PreferredAppsList().FindPreferredAppForUrl(url) ==
            apps[0].launch_name;

    if (apps::features::LinkCapturingInfoBarEnabled() ||
        should_launch_for_preferred_app) {
      LaunchAppFromIntentPicker(web_contents(), url, apps[0].launch_name,
                                apps[0].type);
      return;
    }
  }

  bool show_stay_in_chrome;
  bool show_remember_selection;
#if BUILDFLAG(IS_CHROMEOS)
  show_stay_in_chrome = true;
  show_remember_selection = true;
#else
  show_stay_in_chrome = false;
  show_remember_selection = false;
#endif  // BUILDFLAG(IS_CHROMEOS)

  auto show_intent_picker_bubble = base::BindOnce(
      &ShowIntentPickerBubbleForApps, web_contents(), show_stay_in_chrome,
      show_remember_selection,
      base::BindOnce(&IntentPickerTabHelper::OnIntentPickerClosedMaybeLaunch,
                     per_navigation_weak_factory_.GetWeakPtr(), url));

  LoadAppIcon(std::move(apps),
              /*index=*/0, std::move(show_intent_picker_bubble));
}

void IntentPickerTabHelper::OnIntentPickerClosedMaybeLaunch(
    const GURL& url,
    const std::string& launch_name,
    apps::PickerEntryType entry_type,
    apps::IntentPickerCloseReason close_reason,
    bool should_persist) {
  if (web_contents()->IsBeingDestroyed()) {
    return;
  }

#if BUILDFLAG(IS_CHROMEOS)
  OnIntentPickerClosedChromeOs(web_contents()->GetWeakPtr(), url, launch_name,
                               entry_type, close_reason, should_persist);
#else
  const bool should_launch_app =
      close_reason == apps::IntentPickerCloseReason::OPEN_APP;
  if (should_launch_app) {
    LaunchAppFromIntentPicker(web_contents(), url, launch_name, entry_type);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void IntentPickerTabHelper::SetIconUpdateCallbackForTesting(
    base::OnceClosure callback,
    bool include_latest_navigation) {
  if (icon_resolved_after_last_navigation_ && include_latest_navigation) {
    std::move(callback).Run();
    return;
  }
  icon_update_closure_ = std::move(callback);
}

void IntentPickerTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (IsNavigatingToNewSite(navigation_handle)) {
    icon_resolved_after_last_navigation_ = false;
  }
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

  if (IsNavigatingToNewSite(navigation_handle)) {
    per_navigation_weak_factory_.InvalidateWeakPtrs();

    bool is_valid_page = navigation_handle->GetURL().SchemeIsHTTPOrHTTPS() &&
                         !navigation_handle->IsErrorPage();
    if (is_valid_page) {
      MaybeShowIntentPickerIcon(web_contents());
    } else {
      ShowOrHideIcon(web_contents(), /*should_show_icon=*/false);
    }
  }
}

void IntentPickerTabHelper::OnWebAppWillBeUninstalled(
    const webapps::AppId& app_id) {
  // WebAppTabHelper has an app_id but it is reset during
  // OnWebAppWillBeUninstalled so using FindAppWithUrlInScope.
  absl::optional<webapps::AppId> local_app_id =
      registrar_->FindAppWithUrlInScope(web_contents()->GetLastCommittedURL());
  if (app_id == local_app_id)
    ShowOrHideIcon(web_contents(), /*should_show_icon=*/false);
}

void IntentPickerTabHelper::OnWebAppInstallManagerDestroyed() {
  install_manager_observation_.Reset();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(IntentPickerTabHelper);
