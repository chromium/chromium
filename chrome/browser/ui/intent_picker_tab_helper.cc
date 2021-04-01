// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/intent_picker_tab_helper.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/intent_helper/intent_picker_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image.h"

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

}  // namespace

IntentPickerTabHelper::~IntentPickerTabHelper() = default;

// static
void IntentPickerTabHelper::SetShouldShowIcon(
    content::WebContents* web_contents,
    bool should_show_icon) {
  IntentPickerTabHelper* tab_helper = FromWebContents(web_contents);
  if (!tab_helper)
    return;
  tab_helper->should_show_icon_ = should_show_icon;
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return;
  browser->window()->UpdatePageActionIcon(PageActionIconType::kIntentPicker);
}

IntentPickerTabHelper::IntentPickerTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

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

void IntentPickerTabHelper::OnAppIconLoaded(
    std::vector<apps::IntentPickerAppInfo> apps,
    IntentPickerIconLoaderCallback callback,
    size_t index,
    apps::mojom::IconValuePtr icon_value) {
  apps[index].icon_model =
      ui::ImageModel::FromImage(gfx::Image(icon_value->uncompressed));

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

  constexpr bool allow_placeholder_icon = false;
  auto icon_type =
      (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon))
          ? apps::mojom::IconType::kStandard
          : apps::mojom::IconType::kUncompressed;
  apps::AppServiceProxyFactory::GetForProfile(profile)->LoadIcon(
      app_type, app_id, icon_type, gfx::kFaviconSize, allow_placeholder_icon,
      base::BindOnce(&IntentPickerTabHelper::OnAppIconLoaded,
                     weak_factory_.GetWeakPtr(), std::move(apps),
                     std::move(callback), index));
}

void IntentPickerTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // For a http/https scheme URL navigation, we will check if the
  // url can be handled by some apps, and show intent picker icon
  // or bubble if there are some apps available. We only want to check this if
  // the navigation happens in the main frame, and the navigation is not the
  // same document with same URL.
  // TODO(crbug.com/826982): Check is not error page here. Adding this check
  // will break the browser test, given this is a refactor CL, will add check in
  // follow up CL.
  if (navigation_handle->IsInMainFrame() && navigation_handle->HasCommitted() &&
      (!navigation_handle->IsSameDocument() ||
       navigation_handle->GetURL() !=
           navigation_handle->GetPreviousMainFrameURL()) &&
      navigation_handle->GetURL().SchemeIsHTTPOrHTTPS()) {
    apps::MaybeShowIntentPicker(navigation_handle);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(IntentPickerTabHelper)
