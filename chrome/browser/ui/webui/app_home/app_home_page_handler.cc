// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/webui/app_home/app_home_page_handler.h"

#include "chrome/browser/apps/app_service/app_icon/app_icon_source.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_manager.h"
#include "chrome/browser/ui/web_applications/web_app_ui_manager_impl.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
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

AppHomePageHandler::AppHomePageHandler(
    content::WebUI* web_ui,
    Profile* profile,
    mojo::PendingReceiver<app_home::mojom::PageHandler> receiver,
    mojo::PendingRemote<app_home::mojom::Page> page)
    : web_ui_(web_ui),
      profile_(profile),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      web_app_provider_(web_app::WebAppProvider::GetForWebApps(profile)),
      extension_service_(
          extensions::ExtensionSystem::Get(profile)->extension_service()) {
  install_manager_observation_.Observe(&web_app_provider_->install_manager());
  ExtensionRegistry::Get(profile)->AddObserver(this);
}

AppHomePageHandler::~AppHomePageHandler() {
  ExtensionRegistry::Get(profile_)->RemoveObserver(this);
  // Destroy `extension_uninstall_dialog_` now, since `this` is an
  // `ExtensionUninstallDialog::Delegate` and the dialog may call back into
  // `this` when destroyed.
  extension_uninstall_dialog_.reset();
}

Browser* AppHomePageHandler::GetCurrentBrowser() {
  return chrome::FindBrowserWithWebContents(web_ui_->GetWebContents());
}

app_home::mojom::AppInfoPtr AppHomePageHandler::CreateAppInfoPtrFromWebApp(
    const web_app::AppId& app_id) {
  auto& registrar = web_app_provider_->registrar();

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
  web_app::WebAppRegistrar& registrar = web_app_provider_->registrar();

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

void AppHomePageHandler::OnExtensionUninstallDialogClosed(
    bool did_start_uninstall,
    const std::u16string& error) {
  CleanupAfterUninstall();
}

void AppHomePageHandler::CleanupAfterUninstall() {
  uninstall_dialog_prompting_ = false;
}

void AppHomePageHandler::UninstallWebApp(const std::string& web_app_id) {
  if (!web_app_provider_->install_finalizer().CanUserUninstallWebApp(
          web_app_id)) {
    LOG(ERROR) << "Attempt to uninstall a webapp that is non-usermanagable "
                  "was made. App id : "
               << web_app_id;
    return;
  }

  uninstall_dialog_prompting_ = true;

  auto uninstall_success_callback = base::BindOnce(
      [](base::WeakPtr<AppHomePageHandler> app_home_page_handler,
         webapps::UninstallResultCode code) {
        if (app_home_page_handler) {
          app_home_page_handler->CleanupAfterUninstall();
        }
      },
      weak_ptr_factory_.GetWeakPtr());

  Browser* browser = GetCurrentBrowser();
  web_app::WebAppUiManagerImpl::Get(web_app_provider_)
      ->dialog_manager()
      .UninstallWebApp(web_app_id, webapps::WebappUninstallSource::kAppsPage,
                       browser->window(),
                       std::move(uninstall_success_callback));
  return;
}

extensions::ExtensionUninstallDialog*
AppHomePageHandler::CreateExtensionUninstallDialog() {
  Browser* browser = GetCurrentBrowser();
  extension_uninstall_dialog_ = extensions::ExtensionUninstallDialog::Create(
      extension_service_->profile(), browser->window()->GetNativeWindow(),
      this);
  return extension_uninstall_dialog_.get();
}

void AppHomePageHandler::UninstallExtensionApp(const Extension* extension) {
  if (!extensions::ExtensionSystem::Get(extension_service_->profile())
           ->management_policy()
           ->UserMayModifySettings(extension, nullptr)) {
    LOG(ERROR) << "Attempt to uninstall an extension that is non-usermanagable "
                  "was made. Extension id : "
               << extension->id();
    return;
  }

  uninstall_dialog_prompting_ = true;

  Browser* browser = GetCurrentBrowser();
  extension_uninstall_dialog_ = extensions::ExtensionUninstallDialog::Create(
      extension_service_->profile(), browser->window()->GetNativeWindow(),
      this);

  extension_uninstall_dialog_->ConfirmUninstall(
      extension, extensions::UNINSTALL_REASON_USER_INITIATED,
      extensions::UNINSTALL_SOURCE_CHROME_APPS_PAGE);
}

void AppHomePageHandler::GetApps(GetAppsCallback callback) {
  std::vector<app_home::mojom::AppInfoPtr> result;
  FillWebAppInfoList(&result);
  FillExtensionInfoList(&result);
  std::move(callback).Run(std::move(result));
}

void AppHomePageHandler::OnWebAppWillBeUninstalled(
    const web_app::AppId& app_id) {
  auto app_info = app_home::mojom::AppInfo::New();
  app_info->id = app_id;
  page_->RemoveApp(std::move(app_info));
}

void AppHomePageHandler::OnWebAppInstalled(const web_app::AppId& app_id) {
  page_->AddApp(CreateAppInfoPtrFromWebApp(app_id));
}

void AppHomePageHandler::OnWebAppInstallManagerDestroyed() {
  install_manager_observation_.Reset();
}

void AppHomePageHandler::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  page_->AddApp(CreateAppInfoPtrFromExtension(extension));
}

void AppHomePageHandler::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  auto app_info = app_home::mojom::AppInfo::New();
  app_info->id = extension->id();
  page_->RemoveApp(std::move(app_info));
}

void AppHomePageHandler::UninstallApp(const std::string& app_id) {
  if (uninstall_dialog_prompting_)
    return;

  if (web_app_provider_->registrar().IsInstalled(app_id) &&
      !IsYoutubeExtension(app_id)) {
    UninstallWebApp(app_id);
    return;
  }

  const Extension* extension =
      ExtensionRegistry::Get(extension_service_->profile())
          ->GetInstalledExtension(app_id);
  if (extension) {
    UninstallExtensionApp(extension);
  }
}

}  // namespace webapps
