// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_ui_utils.h"

#include "base/feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/web_applications/web_app_browser_controller.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"

namespace web_app {

namespace {

absl::optional<AppId> GetAppIdForManagementLinkInWebContents(
    content::WebContents* web_contents) {
#if BUILDFLAG(IS_CHROMEOS)
  bool show_app_link_in_app_window = true;
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(crbug.com/1297868): implement `ShowAppManagementPage()` for lacros.
  bool show_app_link_in_app_window = false;
#else
  bool show_app_link_in_app_window =
      base::FeatureList::IsEnabled(features::kDesktopPWAsWebAppSettingsPage);
#endif
  bool show_app_link_in_tabbed_browser =
      show_app_link_in_app_window &&
      base::FeatureList::IsEnabled(blink::features::kFileHandlingAPI);

  if (!show_app_link_in_app_window)
    return absl::nullopt;

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return absl::nullopt;

  if (!web_app::AppBrowserController::IsWebApp(browser) &&
      !show_app_link_in_tabbed_browser) {
    return absl::nullopt;
  }

  WebAppTabHelper* helper = WebAppTabHelper::FromWebContents(web_contents);
  if (!helper || helper->GetAppId().empty() || !helper->acting_as_app())
    return absl::nullopt;

  if (!WebAppProvider::GetForWebApps(browser->profile())
           ->registrar()
           .IsInstalled(helper->GetAppId())) {
    return absl::nullopt;
  }

  return helper->GetAppId();
}

}  // namespace

bool GetLabelIdsForAppManagementLinkInPageInfo(
    content::WebContents* web_contents,
    int* link_text_id,
    int* tooltip_text_id) {
  absl::optional<AppId> app_id =
      GetAppIdForManagementLinkInWebContents(web_contents);
  if (!app_id)
    return false;

  *link_text_id = IDS_WEB_APP_SETTINGS_LINK;
  *tooltip_text_id = IDS_WEB_APP_SETTINGS_LINK_TOOLTIP;
  return true;
}

bool HandleAppManagementLinkClickedInPageInfo(
    content::WebContents* web_contents) {
  absl::optional<AppId> app_id =
      GetAppIdForManagementLinkInWebContents(web_contents);
  if (!app_id)
    return false;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  chrome::ShowAppManagementPage(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()), *app_id,
      ash::settings::AppManagementEntryPoint::kPageInfoView);
  return true;
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(crbug.com/1297868): implement for lacros.
  return false;
#else
  chrome::ShowWebAppSettings(chrome::FindBrowserWithWebContents(web_contents),
                             *app_id, AppSettingsPageEntryPoint::kPageInfoView);
  return true;
#endif
}

}  // namespace web_app
