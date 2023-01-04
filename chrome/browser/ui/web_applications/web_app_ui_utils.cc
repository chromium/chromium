// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_ui_utils.h"

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
#include "chrome/grit/generated_resources.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif

namespace web_app {

namespace {

absl::optional<AppId> GetAppIdForManagementLinkInWebContents(
    content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return absl::nullopt;

  const web_app::AppId* app_id =
      web_app::WebAppTabHelper::GetAppId(web_contents);
  if (!app_id)
    return absl::nullopt;

  if (!web_app::WebAppTabHelper::FromWebContents(web_contents)->acting_as_app())
    return absl::nullopt;

  if (!WebAppProvider::GetForWebApps(browser->profile())
           ->registrar_unsafe()
           .IsInstalled(*app_id)) {
    return absl::nullopt;
  }

  return *app_id;
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void ShowAppManagementPage(const AppId& app_id) {
  auto* service = chromeos::LacrosService::Get();
  if (!service || !service->IsAvailable<crosapi::mojom::AppServiceProxy>()) {
    LOG(ERROR) << "AppServiceProxy not available.";
    return;
  }

  service->GetRemote<crosapi::mojom::AppServiceProxy>()->ShowAppManagementPage(
      app_id);
}
#endif

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
  ShowAppManagementPage(*app_id);
  return true;
#else
  chrome::ShowWebAppSettings(chrome::FindBrowserWithWebContents(web_contents),
                             *app_id, AppSettingsPageEntryPoint::kPageInfoView);
  return true;
#endif
}

}  // namespace web_app
