// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_file_handler_manager.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_icon_manager.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_install_finalizer.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_registrar.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_registry_controller.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_shortcut_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace web_app {

void WebAppProvider::CreateBookmarkAppsSubsystems(Profile* profile) {
  std::unique_ptr<extensions::BookmarkAppRegistrar> registrar =
      std::make_unique<extensions::BookmarkAppRegistrar>(profile);
  std::unique_ptr<extensions::BookmarkAppRegistryController>
      registry_controller =
          std::make_unique<extensions::BookmarkAppRegistryController>(
              profile, registrar.get());
  icon_manager_ = std::make_unique<extensions::BookmarkAppIconManager>(profile);
  install_finalizer_ =
      std::make_unique<extensions::BookmarkAppInstallFinalizer>(profile);

  auto file_handler_manager =
      std::make_unique<extensions::BookmarkAppFileHandlerManager>(profile);
  auto shortcut_manager =
      std::make_unique<extensions::BookmarkAppShortcutManager>(profile);
  os_integration_manager_ = std::make_unique<OsIntegrationManager>(
      profile, std::move(shortcut_manager), std::move(file_handler_manager),
      /*protocol_handler_manager*/ nullptr, /*url_handler_manager*/ nullptr);

  // Upcast to unified subsystem types:
  registrar_ = std::move(registrar);
  registry_controller_ = std::move(registry_controller);
}

std::unique_ptr<InstallFinalizer>
WebAppProvider::CreateBookmarkAppInstallFinalizer(Profile* profile) {
  return std::make_unique<extensions::BookmarkAppInstallFinalizer>(profile);
}

void WebAppProviderFactory::DependsOnExtensionsSystem() {
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

}  // namespace web_app
