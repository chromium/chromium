// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/file_handler_manager.h"

#include "base/feature_list.h"
#include "chrome/browser/web_applications/components/web_app_file_handler_registration.h"
#include "third_party/blink/public/common/features.h"

namespace web_app {

FileHandlerManager::FileHandlerManager(Profile* profile)
    : profile_(profile), registrar_observer_(this), shortcut_observer_(this) {}

FileHandlerManager::~FileHandlerManager() = default;

void FileHandlerManager::SetSubsystems(AppRegistrar* registrar,
                                       AppShortcutManager* shortcut_manager) {
  registrar_ = registrar;
  shortcut_manager_ = shortcut_manager;
}

void FileHandlerManager::Start() {
  DCHECK(registrar_);
  DCHECK(shortcut_manager_);

  registrar_observer_.Add(registrar_);
  shortcut_observer_.Add(shortcut_manager_);
}

void FileHandlerManager::OnWebAppUninstalled(const AppId& installed_app_id) {
  if (base::FeatureList::IsEnabled(blink::features::kFileHandlingAPI) &&
      OsSupportsWebAppFileHandling()) {
    UnregisterFileHandlersForWebApp(installed_app_id, profile_);
  }
}

void FileHandlerManager::OnWebAppProfileWillBeDeleted(const AppId& app_id) {
  if (base::FeatureList::IsEnabled(blink::features::kFileHandlingAPI) &&
      OsSupportsWebAppFileHandling()) {
    UnregisterFileHandlersForWebApp(app_id, profile_);
  }
}

void FileHandlerManager::OnAppRegistrarDestroyed() {
  registrar_observer_.RemoveAll();
}

void FileHandlerManager::OnShortcutsCreated(const AppId& installed_app_id) {
  if (!base::FeatureList::IsEnabled(blink::features::kFileHandlingAPI) ||
      !OsSupportsWebAppFileHandling()) {
    return;
  }

  std::string app_name = registrar_->GetAppShortName(installed_app_id);
  const std::vector<apps::FileHandlerInfo>* file_handlers =
      GetFileHandlers(installed_app_id);
  if (!file_handlers)
    return;
  std::set<std::string> file_extensions =
      GetFileExtensionsFromFileHandlers(*file_handlers);
  std::set<std::string> mime_types =
      GetMimeTypesFromFileHandlers(*file_handlers);
  RegisterFileHandlersForWebApp(installed_app_id, app_name, profile_,
                                file_extensions, mime_types);
}

void FileHandlerManager::OnShortcutManagerDestroyed() {
  shortcut_observer_.RemoveAll();
}

std::set<std::string> GetFileExtensionsFromFileHandlers(
    const std::vector<apps::FileHandlerInfo>& file_handlers) {
  std::set<std::string> file_extensions;
  for (const auto& file_handler : file_handlers) {
    for (const auto& file_ext : file_handler.extensions)
      file_extensions.insert(file_ext);
  }
  return file_extensions;
}

std::set<std::string> GetMimeTypesFromFileHandlers(
    const std::vector<apps::FileHandlerInfo>& file_handlers) {
  std::set<std::string> mime_types;
  for (const auto& file_handler : file_handlers) {
    for (const auto& mime_type : file_handler.types)
      mime_types.insert(mime_type);
  }
  return mime_types;
}

}  // namespace web_app
