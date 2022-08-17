// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/webui/app_home/app_home_page_handler.h"

#include "chrome/browser/apps/app_service/app_icon/app_icon_source.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/extension_registry.h"
#include "url/gurl.h"

using content::WebUI;
using extensions::Extension;
using extensions::ExtensionRegistry;
using extensions::ExtensionSet;

namespace webapps {

namespace {

const int kWebAppLargeIconSize = 128;

// The Youtube app is incorrectly hardcoded to be a 'bookmark app'. However, it
// is a platform app.
// TODO(crbug.com/1065748): Remove this hack once the youtube app is fixed.
bool IsYoutubeExtension(const std::string& extension_id) {
  return extension_id == extension_misc::kYoutubeAppId;
}
}  // namespace

AppHomePageHandler::~AppHomePageHandler() = default;

AppHomePageHandler::AppHomePageHandler(
    content::WebUI* web_ui,
    Profile* profile,
    mojo::PendingReceiver<app_home::mojom::PageHandler> receiver,
    mojo::PendingRemote<app_home::mojom::Page> page)
    : web_ui_(web_ui),
      profile_(profile),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {}

app_home::mojom::AppInfoPtr AppHomePageHandler::CreateAppInfoPtrFromWebApp(
    const web_app::AppId& app_id) {
  auto& registrar =
      web_app::WebAppProvider::GetForWebApps(profile_)->registrar();

  auto app_info = app_home::mojom::AppInfo::New();

  app_info->id = app_id;

  GURL start_url = registrar.GetAppStartUrl(app_id);
  app_info->start_url = start_url;

  std::string name = registrar.GetAppShortName(app_id);
  app_info->name = name;

  app_info->icon_url =
      apps::AppIconSource::GetIconURL(app_id, kWebAppLargeIconSize);

  return app_info;
}

app_home::mojom::AppInfoPtr AppHomePageHandler::CreateAppInfoPtrFromExtension(
    const Extension* extension) {
  auto app_info = app_home::mojom::AppInfo::New();

  app_info->id = extension->id();

  GURL start_url = extensions::AppLaunchInfo::GetFullLaunchURL(extension);
  app_info->start_url = start_url;

  app_info->name = extension->name();

  app_info->icon_url = extensions::ExtensionIconSource::GetIconURL(
      extension, extension_misc::EXTENSION_ICON_LARGE,
      ExtensionIconSet::MATCH_BIGGER, false /*grayscale*/);

  return app_info;
}

void AppHomePageHandler::FillWebAppInfoList(
    std::vector<app_home::mojom::AppInfoPtr>* result) {
  web_app::WebAppRegistrar& registrar =
      web_app::WebAppProvider::GetForWebApps(profile_)->registrar();

  for (const web_app::AppId& web_app_id : registrar.GetAppIds()) {
    if (IsYoutubeExtension(web_app_id))
      continue;
    result->emplace_back(CreateAppInfoPtrFromWebApp(web_app_id));
  }
}

void AppHomePageHandler::FillExtensionInfoList(
    std::vector<app_home::mojom::AppInfoPtr>* result) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
  std::unique_ptr<ExtensionSet> extension_apps =
      registry->GenerateInstalledExtensionsSet(ExtensionRegistry::ENABLED |
                                               ExtensionRegistry::DISABLED |
                                               ExtensionRegistry::TERMINATED);
  for (const auto& extension : *extension_apps) {
    if (extensions::ui_util::ShouldDisplayInNewTabPage(extension.get(),
                                                       profile_))
      result->emplace_back(CreateAppInfoPtrFromExtension(extension.get()));
  }
}

void AppHomePageHandler::GetApps(GetAppsCallback callback) {
  std::vector<app_home::mojom::AppInfoPtr> result;
  FillWebAppInfoList(&result);
  FillExtensionInfoList(&result);
  std::move(callback).Run(std::move(result));
}

}  // namespace webapps
