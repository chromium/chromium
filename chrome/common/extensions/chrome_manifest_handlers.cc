// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/chrome_manifest_handlers.h"

#include <memory>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/extensions/chrome_manifest_url_handlers.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/extensions/manifest_handlers/minimum_chrome_version_checker.h"
#include "chrome/common/extensions/manifest_handlers/natively_connectable_handler.h"
#include "chrome/common/extensions/manifest_handlers/settings_overrides_handler.h"
#include "chrome/common/extensions/manifest_handlers/theme_handler.h"
#include "extensions/common/manifest_handler_registry.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/common/extensions/api/omnibox/omnibox_handler.h"
#include "chrome/common/extensions/api/side_panel/side_panel_info.h"
#include "chrome/common/extensions/api/speech/tts_engine_manifest_handler.h"
#include "chrome/common/extensions/api/storage/storage_schema_manifest_handler.h"
#include "chrome/common/extensions/api/system_indicator/system_indicator_handler.h"
#include "chrome/common/extensions/api/url_handlers/url_handlers_parser.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/common/extensions/api/file_browser_handlers/file_browser_handler.h"
#include "chrome/common/extensions/api/file_system_provider_capabilities/file_system_provider_capabilities_handler.h"
#endif

namespace extensions {

void RegisterChromeManifestHandlers() {
  // TODO(devlin): Pass in |registry| rather than Get()ing it.
  ManifestHandlerRegistry* registry = ManifestHandlerRegistry::Get();

  registry->RegisterHandler(std::make_unique<AppLaunchManifestHandler>());
  registry->RegisterHandler(std::make_unique<DevToolsPageHandler>());
  registry->RegisterHandler(std::make_unique<MinimumChromeVersionChecker>());
  registry->RegisterHandler(std::make_unique<NativelyConnectableHandler>());
  registry->RegisterHandler(std::make_unique<SettingsOverridesHandler>());
  registry->RegisterHandler(std::make_unique<ThemeHandler>());
  registry->RegisterHandler(std::make_unique<URLOverridesHandler>());

#if BUILDFLAG(ENABLE_EXTENSIONS)
  registry->RegisterHandler(std::make_unique<OmniboxHandler>());
  registry->RegisterHandler(std::make_unique<SidePanelManifestHandler>());
  registry->RegisterHandler(std::make_unique<StorageSchemaManifestHandler>());
  registry->RegisterHandler(std::make_unique<SystemIndicatorHandler>());
  registry->RegisterHandler(std::make_unique<TtsEngineManifestHandler>());
  registry->RegisterHandler(std::make_unique<UrlHandlersParser>());
#endif

#if BUILDFLAG(IS_CHROMEOS)
  registry->RegisterHandler(std::make_unique<FileBrowserHandlerParser>());
  registry->RegisterHandler(
      std::make_unique<FileSystemProviderCapabilitiesHandler>());
#endif
}

}  // namespace extensions
