// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_file_handler_manager.h"

#include <set>
#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_registration.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace web_app {

namespace {

// Used to enable running tests on platforms that don't support file handling
// icons.
std::optional<bool> g_icons_supported_by_os_override;

}  // namespace

WebAppFileHandlerManager::WebAppFileHandlerManager(Profile* profile)
    : profile_(profile) {}

WebAppFileHandlerManager::~WebAppFileHandlerManager() = default;

void WebAppFileHandlerManager::SetProvider(base::PassKey<OsIntegrationManager>,
                                           WebAppProvider& provider) {
  provider_ = &provider;
}

void WebAppFileHandlerManager::Start() {
  DCHECK(provider_);
}

// static
void WebAppFileHandlerManager::SetIconsSupportedByOsForTesting(bool value) {
  g_icons_supported_by_os_override = value;
}

const apps::FileHandlers* WebAppFileHandlerManager::GetEnabledFileHandlers(
    const webapps::AppId& app_id) const {
  if (ShouldOsIntegrationBeEnabled(app_id) &&
      !provider_->registrar_unsafe().IsAppFileHandlerPermissionBlocked(
          app_id)) {
    return GetAllFileHandlers(app_id);
  }

  return nullptr;
}

// static
bool WebAppFileHandlerManager::IconsEnabled() {
  return g_icons_supported_by_os_override.value_or(
             FileHandlingIconsSupportedByOs()) &&
         base::FeatureList::IsEnabled(blink::features::kFileHandlingIcons);
}

const apps::FileHandlers* WebAppFileHandlerManager::GetAllFileHandlers(
    const webapps::AppId& app_id) const {
  const WebApp* web_app = provider_->registrar_unsafe().GetAppById(app_id);
  return web_app && !web_app->file_handlers().empty()
             ? &web_app->file_handlers()
             : nullptr;
}

bool WebAppFileHandlerManager::IsDisabledForTesting() {
  return false;
}

WebAppFileHandlerManager::LaunchInfos
WebAppFileHandlerManager::GetMatchingFileHandlerUrls(
    const webapps::AppId& app_id,
    const std::vector<base::FilePath>& launch_files) {
  LaunchInfos launch_infos;
  if (launch_files.empty() ||
      provider_->registrar_unsafe().IsAppFileHandlerPermissionBlocked(app_id)) {
    return launch_infos;
  }

  std::map<const apps::FileHandler*, std::vector<base::FilePath>>
      launch_handlers;

  const apps::FileHandlers* file_handlers = GetAllFileHandlers(app_id);
  if (!file_handlers)
    return launch_infos;

  for (const auto& file_path : launch_files) {
    std::string file_extension =
        base::FilePath(file_path.Extension()).AsUTF8Unsafe();
    if (file_extension.length() <= 1)
      continue;

    for (const auto& file_handler : *file_handlers) {
      std::set<std::string> supported_file_extensions =
          apps::GetFileExtensionsFromFileHandlers({file_handler});
      if (base::Contains(supported_file_extensions,
                         base::ToLowerASCII(file_extension))) {
        launch_handlers[&file_handler].push_back(file_path);
        break;
      }
    }
  }

  for (auto& launch_handler : launch_handlers) {
    const GURL& action = launch_handler.first->action;
    if (launch_handler.first->launch_type ==
        apps::FileHandler::LaunchType::kMultipleClients) {
      for (base::FilePath& file : launch_handler.second) {
        launch_infos.emplace_back(action,
                                  std::vector<base::FilePath>{std::move(file)});
      }
    } else {
      launch_infos.emplace_back(action, std::move(launch_handler.second));
    }
  }

  return launch_infos;
}

bool WebAppFileHandlerManager::ShouldOsIntegrationBeEnabled(
    const webapps::AppId& app_id) const {
  return !ShouldRegisterFileHandlersWithOs() ||
         (provider_ && provider_->registrar_unsafe()
                           .ExpectThatFileHandlersAreRegisteredWithOs(app_id));
}

}  // namespace web_app
