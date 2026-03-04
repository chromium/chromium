// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sub_apps_permission_explanation.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/grit/generated_resources.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

std::optional<std::u16string> GetSubAppsPermissionExplanation(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return std::nullopt;
  }

  const webapps::AppId* app_id_ptr =
      web_app::WebAppTabHelper::GetAppId(web_contents);
  if (!app_id_ptr) {
    return std::nullopt;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
  if (!provider) {
    return std::nullopt;
  }

  const web_app::WebAppRegistrar& registrar = provider->registrar_unsafe();

  // Show if it's an isolated sub app.
  if (registrar.AppMatches(*app_id_ptr,
                           web_app::WebAppFilter::IsIsolatedSubApp())) {
    const web_app::WebApp* app = registrar.GetAppById(*app_id_ptr);
    if (app && app->parent_app_id().has_value()) {
      std::string app_name = registrar.GetAppShortName(*app_id_ptr);
      std::string parent_app_name =
          registrar.GetAppShortName(app->parent_app_id().value());
      return l10n_util::GetStringFUTF16(
          IDS_APP_MANAGEMENT_IS_SUB_APP_PERMISSION_EXPLANATION,
          base::UTF8ToUTF16(app_name), base::UTF8ToUTF16(parent_app_name));
    }
  }

  // Show if it's an isolated web app that has sub apps.
  if (registrar.AppMatches(*app_id_ptr,
                           web_app::WebAppFilter::IsIsolatedApp())) {
    if (!registrar.GetAllSubAppIds(*app_id_ptr).empty()) {
      std::string app_name = registrar.GetAppShortName(*app_id_ptr);
      return l10n_util::GetStringFUTF16(
          IDS_APP_MANAGEMENT_HAS_SUB_APPS_PERMISSION_EXPLANATION,
          base::UTF8ToUTF16(app_name));
    }
  }

  return std::nullopt;
}
