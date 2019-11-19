// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/chrome_manifest_handlers.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/common/extensions/api/commands/commands_handler.h"
#include "chrome/common/extensions/api/omnibox/omnibox_handler.h"
#include "chrome/common/extensions/api/speech/tts_engine_manifest_handler.h"
#include "chrome/common/extensions/api/spellcheck/spellcheck_handler.h"
#include "chrome/common/extensions/api/storage/storage_schema_manifest_handler.h"
#include "chrome/common/extensions/api/system_indicator/system_indicator_handler.h"
#include "chrome/common/extensions/api/url_handlers/url_handlers_parser.h"
#include "chrome/common/extensions/chrome_manifest_url_handlers.h"
#include "chrome/common/extensions/manifest_handlers/app_display_mode_info.h"
#include "chrome/common/extensions/manifest_handlers/app_icon_color_info.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/extensions/manifest_handlers/app_theme_color_info.h"
#include "chrome/common/extensions/manifest_handlers/extension_action_handler.h"
#include "chrome/common/extensions/manifest_handlers/linked_app_icons.h"
#include "chrome/common/extensions/manifest_handlers/minimum_chrome_version_checker.h"
#include "chrome/common/extensions/manifest_handlers/natively_connectable_handler.h"
#include "chrome/common/extensions/manifest_handlers/settings_overrides_handler.h"
#include "chrome/common/extensions/manifest_handlers/theme_handler.h"
#include "chrome/common/extensions/manifest_handlers/ui_overrides_handler.h"
#include "extensions/common/manifest_handlers/app_isolation_info.h"
#include "extensions/common/manifest_handlers/automation.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/manifest_url_handlers.h"

#if defined(OS_CHROMEOS)
#include "chrome/common/extensions/api/file_browser_handlers/file_browser_handler.h"
#include "chrome/common/extensions/api/file_system_provider_capabilities/file_system_provider_capabilities_handler.h"
#include "chrome/common/extensions/api/input_ime/input_components_handler.h"
#endif

namespace extensions {

void RegisterChromeManifestHandlers() {
  // TODO(devlin): Pass in |registry| rather than Get()ing it.
  ManifestHandlerRegistry* registry = ManifestHandlerRegistry::Get();

  DCHECK(!ManifestHandler::IsRegistrationFinalized());

  registry->RegisterHandler(std::make_unique<AboutPageHandler>());
  registry->RegisterHandler(std::make_unique<AppDisplayModeHandler>());
  registry->RegisterHandler(std::make_unique<AppIconColorHandler>());
  registry->RegisterHandler(std::make_unique<AppThemeColorHandler>());
  registry->RegisterHandler(std::make_unique<AppIsolationHandler>());
  registry->RegisterHandler(std::make_unique<AppLaunchManifestHandler>());
  registry->RegisterHandler(std::make_unique<AutomationHandler>());
  registry->RegisterHandler(std::make_unique<CommandsHandler>());
  registry->RegisterHandler(std::make_unique<DevToolsPageHandler>());
  registry->RegisterHandler(std::make_unique<ExtensionActionHandler>());
  registry->RegisterHandler(std::make_unique<HomepageURLHandler>());
  registry->RegisterHandler(std::make_unique<LinkedAppIconsHandler>());
  registry->RegisterHandler(std::make_unique<MinimumChromeVersionChecker>());
  registry->RegisterHandler(std::make_unique<NativelyConnectableHandler>());
  registry->RegisterHandler(std::make_unique<OmniboxHandler>());
  registry->RegisterHandler(std::make_unique<OptionsPageManifestHandler>());
  registry->RegisterHandler(std::make_unique<SettingsOverridesHandler>());
  registry->RegisterHandler(std::make_unique<SpellcheckHandler>());
  registry->RegisterHandler(std::make_unique<StorageSchemaManifestHandler>());
  registry->RegisterHandler(std::make_unique<SystemIndicatorHandler>());
  registry->RegisterHandler(std::make_unique<ThemeHandler>());
  registry->RegisterHandler(std::make_unique<TtsEngineManifestHandler>());
  registry->RegisterHandler(std::make_unique<UIOverridesHandler>());
  registry->RegisterHandler(std::make_unique<UrlHandlersParser>());
  registry->RegisterHandler(std::make_unique<URLOverridesHandler>());
#if defined(OS_CHROMEOS)
  registry->RegisterHandler(std::make_unique<FileBrowserHandlerParser>());
  registry->RegisterHandler(
      std::make_unique<FileSystemProviderCapabilitiesHandler>());
  registry->RegisterHandler(std::make_unique<InputComponentsHandler>());
#endif
}

}  // namespace extensions
