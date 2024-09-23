// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_service/lacros_web_apps_controller.h"

#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/one_shot_event.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/menu_item_constants.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance_tracker.h"
#include "chrome/browser/profiles/profile.h"
// TODO(crbug.com/40251079): Remove circular dependencies on //c/b/ui.
#include "chrome/browser/ui/startup/first_run_service.h"  // nogncheck
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/services/app_service/public/cpp/capability_access.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/events/event_constants.h"
#include "url/gurl.h"

using apps::IconEffects;

namespace {

// Callback to run to finish a controller command. At the end of the command,
// web contents created by it (if any) should be returned to crosapi by calling
// this callback. See `LacrosWebAppsController::ReturnLaunchResults()` for more
// details.
using CommandFinishedCallback =
    base::OnceCallback<void(std::vector<content::WebContents*>)>;

// Helper to run `execute_command_callback`, with the option to bypass it if
// `proceed` is false by running `command_finished_callback` right away and
// returning.
void OnOpenPrimaryProfileFirstRunExited(
    CommandFinishedCallback command_finished_callback,
    base::OnceCallback<void(CommandFinishedCallback)> execute_command_callback,
    bool proceed) {
  if (!proceed) {
    std::move(command_finished_callback).Run({});
  } else {
    std::move(execute_command_callback)
        .Run(std::move(command_finished_callback));
  }
}

}  // namespace

namespace web_app {

LacrosWebAppsController::LacrosWebAppsController(Profile* profile)
    : profile_(profile),
      provider_(WebAppProvider::GetForWebApps(profile)),
      publisher_helper_(profile, provider_, this) {
  DCHECK(provider_);
  DCHECK_EQ(publisher_helper_.app_type(), apps::AppType::kWeb);
}

LacrosWebAppsController::~LacrosWebAppsController() = default;

void LacrosWebAppsController::Init() {
  if (!remote_publisher_) {
    auto* service = chromeos::LacrosService::Get();
    if (!service) {
      return;
    }
    if (!IsWebAppsCrosapiEnabled()) {
      return;
    }

    remote_publisher_version_ =
        service->GetInterfaceVersion<crosapi::mojom::AppPublisher>();

    service->GetRemote<crosapi::mojom::AppPublisher>()->RegisterAppController(
        receiver_.BindNewPipeAndPassRemoteWithVersion());
    remote_publisher_ =
        service->GetRemote<crosapi::mojom::AppPublisher>().get();
  }

  provider_->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&LacrosWebAppsController::OnReady,
                                weak_ptr_factory_.GetWeakPtr()));
}

void LacrosWebAppsController::Shutdown() {
  publisher_helper().Shutdown();
}

WebAppRegistrar& LacrosWebAppsController::registrar() const {
  return provider_->registrar_unsafe();
}

void LacrosWebAppsController::SetPublisherForTesting(
    crosapi::mojom::AppPublisher* publisher) {
  remote_publisher_ = publisher;
  // Set the publisher version to the newest version for testing.
  remote_publisher_version_ = crosapi::mojom::AppPublisher::Version_;
}

void LacrosWebAppsController::OnReady() {
  if (!remote_publisher_ || publisher_helper().IsShuttingDown()) {
    return;
  }

  std::vector<apps::AppPtr> apps;
  for (const WebApp& web_app : registrar().GetApps()) {
    apps.push_back(publisher_helper().CreateWebApp(&web_app));
  }
  PublishWebApps(std::move(apps));
}

void LacrosWebAppsController::Uninstall(const std::string& app_id,
                                        apps::UninstallSource uninstall_source,
                                        bool clear_site_data,
                                        bool report_abuse) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  publisher_helper().UninstallWebApp(web_app, uninstall_source, clear_site_data,
                                     report_abuse);
}

void LacrosWebAppsController::PauseApp(const std::string& app_id) {
  publisher_helper().PauseApp(app_id);
}

void LacrosWebAppsController::UnpauseApp(const std::string& app_id) {
  publisher_helper().UnpauseApp(app_id);
}

void LacrosWebAppsController::DEPRECATED_LoadIcon(
    const std::string& app_id,
    apps::IconKeyPtr icon_key,
    apps::IconType icon_type,
    int32_t size_hint_in_dip,
    apps::LoadIconCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void LacrosWebAppsController::GetCompressedIcon(
    const std::string& app_id,
    int32_t size_in_dip,
    ui::ResourceScaleFactor scale_factor,
    apps::LoadIconCallback callback) {
  publisher_helper().GetCompressedIconData(app_id, size_in_dip, scale_factor,
                                           std::move(callback));
}

void LacrosWebAppsController::OpenNativeSettings(const std::string& app_id) {
  publisher_helper().OpenNativeSettings(app_id);
}

void LacrosWebAppsController::UpdateAppSize(const std::string& app_id) {
  return publisher_helper().UpdateAppSize(app_id);
}

void LacrosWebAppsController::SetWindowMode(const std::string& app_id,
                                            apps::WindowMode window_mode) {
  return publisher_helper().SetWindowMode(app_id, window_mode);
}

void LacrosWebAppsController::GetMenuModel(const std::string& app_id,
                                           GetMenuModelCallback callback) {
  const WebApp* web_app = GetWebApp(app_id);
  auto menu_items = crosapi::mojom::MenuItems::New();
  if (!web_app) {
    std::move(callback).Run(std::move(menu_items));
    return;
  }

  // Read shortcuts menu item icons from disk, if any.
  if (!web_app->shortcuts_menu_item_infos().empty()) {
    provider_->icon_manager().ReadAllShortcutsMenuIcons(
        app_id,
        base::BindOnce(&LacrosWebAppsController::OnShortcutsMenuIconsRead,
                       weak_ptr_factory_.GetWeakPtr(), app_id,
                       std::move(menu_items), std::move(callback)));
  } else {
    std::move(callback).Run(std::move(menu_items));
  }
}

void LacrosWebAppsController::ExecuteContextMenuCommand(
    const std::string& app_id,
    const std::string& id,
    ExecuteContextMenuCommandCallback mojo_callback) {
  auto execution_finished_callback =
      base::BindOnce(&LacrosWebAppsController::ReturnLaunchResults,
                     weak_ptr_factory_.GetWeakPtr(), std::move(mojo_callback));

  auto* fre_service = FirstRunServiceFactory::GetForBrowserContext(profile_);
  if (!fre_service || !fre_service->ShouldOpenFirstRun()) {
    ExecuteContextMenuCommandInternal(app_id, id,
                                      std::move(execution_finished_callback));
    return;
  }

  fre_service->OpenFirstRunIfNeeded(
      FirstRunService::EntryPoint::kWebAppContextMenu,
      base::BindOnce(
          &OnOpenPrimaryProfileFirstRunExited,
          std::move(execution_finished_callback),
          base::BindOnce(
              &LacrosWebAppsController::ExecuteContextMenuCommandInternal,
              weak_ptr_factory_.GetWeakPtr(), app_id, id)));
}

void LacrosWebAppsController::ExecuteContextMenuCommandInternal(
    const std::string& app_id,
    const std::string& id,
    CommandFinishedCallback callback) {
  // CommandFinishedCallback needs a vector, so this lambda is an adapter to
  // transform a single WebContents into a vector.
  publisher_helper().ExecuteContextMenuCommand(
      app_id, id, display::kDefaultDisplayId,
      base::BindOnce(
          [](base::OnceCallback<void(std::vector<content::WebContents*>)>
                 callback,
             content::WebContents* contents) {
            // These calls are piped through LaunchWebAppCommand and can end
            // early during an Abort due to various reasons (like
            // FirstRunService not completed), in which case there will be no
            // web contents.
            if (contents) {
              std::move(callback).Run({contents});
            } else {
              std::move(callback).Run({});
            }
          },
          std::move(callback)));
}

void LacrosWebAppsController::StopApp(const std::string& app_id) {
  publisher_helper().StopApp(app_id);
}

void LacrosWebAppsController::SetPermission(const std::string& app_id,
                                            apps::PermissionPtr permission) {
  publisher_helper().SetPermission(app_id, std::move(permission));
}

// TODO(crbug.com/40155636): Clean up the multiple launch interfaces and remove
// duplicated code.
void LacrosWebAppsController::Launch(
    crosapi::mojom::LaunchParamsPtr launch_params,
    LaunchCallback mojo_callback) {
  if (launch_params->intent) {
    if (!profile_) {
      ReturnLaunchResults(std::move(mojo_callback), {});
      return;
    }
  }

  auto launch_finished_callback =
      base::BindOnce(&LacrosWebAppsController::ReturnLaunchResults,
                     weak_ptr_factory_.GetWeakPtr(), std::move(mojo_callback));
  auto params = apps::ConvertCrosapiToLaunchParams(launch_params, profile_);
  auto* fre_service = FirstRunServiceFactory::GetForBrowserContext(profile_);

  if (!fre_service || !fre_service->ShouldOpenFirstRun()) {
    LaunchInternal(launch_params->app_id, std::move(params),
                   std::move(launch_finished_callback));
    return;
  }

  fre_service->OpenFirstRunIfNeeded(
      FirstRunService::EntryPoint::kWebAppLaunch,
      base::BindOnce(&OnOpenPrimaryProfileFirstRunExited,
                     std::move(launch_finished_callback),
                     base::BindOnce(&LacrosWebAppsController::LaunchInternal,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    launch_params->app_id, std::move(params))));
}

void LacrosWebAppsController::LaunchInternal(const std::string& app_id,
                                             apps::AppLaunchParams params,
                                             CommandFinishedCallback callback) {
  bool is_file_handling_launch =
      !params.launch_files.empty() &&
      !(params.intent && params.intent->IsShareIntent());
  if (is_file_handling_launch) {
    // File handling may create the WebContents asynchronously.
    publisher_helper().LaunchAppWithFilesCheckingUserPermission(
        app_id, std::move(params), std::move(callback));
    return;
  }

  // CommandFinishedCallback needs a vector, so this lambda is an adapter to
  // transform a single WebContents into a vector.
  publisher_helper().LaunchAppWithParams(
      std::move(params),
      base::BindOnce(
          [](base::OnceCallback<void(std::vector<content::WebContents*>)>
                 callback,
             content::WebContents* contents) {
            // These calls are piped through LaunchWebAppCommand and can end
            // early during an Abort due to various reasons (like
            // FirstRunService not completed), in which case there will be no
            // web contents.
            if (contents) {
              std::move(callback).Run({contents});
            } else {
              std::move(callback).Run({});
            }
          },
          std::move(callback)));
}

void LacrosWebAppsController::ReturnLaunchResults(
    base::OnceCallback<void(crosapi::mojom::LaunchResultPtr)> callback,
    std::vector<content::WebContents*> web_contentses) {
  auto* app_instance_tracker =
      apps::AppServiceProxyFactory::GetForProfile(profile_)
          ->BrowserAppInstanceTracker();
  auto launch_result = crosapi::mojom::LaunchResult::New();
  launch_result->instance_id = base::UnguessableToken::Create();
  launch_result->instance_ids = std::vector<base::UnguessableToken>();
  launch_result->state = web_contentses.size()
                             ? crosapi::mojom::LaunchResultState::kSuccess
                             : crosapi::mojom::LaunchResultState::kFailed;

  // TODO(crbug.com/40155636): Replaced with DCHECK when the app instance
  // tracker flag is turned on.
  if (app_instance_tracker) {
    for (content::WebContents* web_contents : web_contentses) {
      const apps::BrowserAppInstance* app_instance =
          app_instance_tracker->GetAppInstance(web_contents);
      if (app_instance) {
        launch_result->instance_ids->push_back(app_instance->id);
      }
    }
  }
  std::move(callback).Run(std::move(launch_result));
}

void LacrosWebAppsController::OnShortcutsMenuIconsRead(
    const std::string& app_id,
    crosapi::mojom::MenuItemsPtr menu_items,
    GetMenuModelCallback callback,
    ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    std::move(callback).Run(crosapi::mojom::MenuItems::New());
    return;
  }

  size_t menu_item_index = 0;

  for (const WebAppShortcutsMenuItemInfo& menu_item_info :
       web_app->shortcuts_menu_item_infos()) {
    const std::map<SquareSizePx, SkBitmap>* menu_item_icon_bitmaps = nullptr;
    if (menu_item_index < shortcuts_menu_icon_bitmaps.size()) {
      // We prefer |MASKABLE| icons, but fall back to icons with purpose |ANY|.
      menu_item_icon_bitmaps =
          &shortcuts_menu_icon_bitmaps[menu_item_index].maskable;
      if (menu_item_icon_bitmaps->empty()) {
        menu_item_icon_bitmaps =
            &shortcuts_menu_icon_bitmaps[menu_item_index].any;
      }
    }

    gfx::ImageSkia icon;
    if (menu_item_icon_bitmaps) {
      icon = apps::ConvertIconBitmapsToImageSkia(
          *menu_item_icon_bitmaps,
          /*size_hint_in_dip=*/apps::kAppShortcutIconSizeDip);
    }

    auto menu_item = crosapi::mojom::MenuItem::New();
    menu_item->label = base::UTF16ToUTF8(menu_item_info.name);
    menu_item->image = icon;
    std::string shortcut_id = publisher_helper().GenerateShortcutId();
    publisher_helper().StoreShortcutId(shortcut_id, menu_item_info);
    menu_item->id = shortcut_id;
    menu_items->items.push_back(std::move(menu_item));

    ++menu_item_index;
  }

  std::move(callback).Run(std::move(menu_items));
}

const WebApp* LacrosWebAppsController::GetWebApp(
    const webapps::AppId& app_id) const {
  return registrar().GetAppById(app_id);
}

void LacrosWebAppsController::PublishWebApps(std::vector<apps::AppPtr> apps) {
  if (!remote_publisher_) {
    return;
  }

  remote_publisher_->OnApps(std::move(apps));
}

void LacrosWebAppsController::PublishWebApp(apps::AppPtr app) {
  if (!remote_publisher_) {
    return;
  }

  std::vector<apps::AppPtr> apps;
  apps.push_back(std::move(app));
  PublishWebApps(std::move(apps));
}

void LacrosWebAppsController::ModifyWebAppCapabilityAccess(
    const std::string& app_id,
    std::optional<bool> accessing_camera,
    std::optional<bool> accessing_microphone) {
  if (!remote_publisher_) {
    return;
  }

  if (!accessing_camera.has_value() && !accessing_microphone.has_value()) {
    return;
  }

  std::vector<apps::CapabilityAccessPtr> capability_accesses;
  auto capability_access = std::make_unique<apps::CapabilityAccess>(app_id);
  capability_access->camera = accessing_camera;
  capability_access->microphone = accessing_microphone;
  capability_accesses.push_back(std::move(capability_access));

  remote_publisher_->OnCapabilityAccesses(std::move(capability_accesses));
}

}  // namespace web_app
