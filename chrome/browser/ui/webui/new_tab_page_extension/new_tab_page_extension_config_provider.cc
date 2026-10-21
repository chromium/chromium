// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page_extension/new_tab_page_extension_config_provider.h"

#include <memory>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "components/contextual_search/contextual_search_service.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_config_map.h"
#include "extensions/browser/extension_config_map_factory.h"
#include "extensions/common/constants.h"
#include "ui/base/webui/web_ui_util.h"

// static
void NewTabPageExtensionConfigProvider::Register(
    content::BrowserContext* browser_context) {
  extensions::ExtensionConfigMapFactory::GetOrCreateForBrowserContext(
      browser_context)
      ->RegisterConfigProvider(
          std::make_unique<NewTabPageExtensionConfigProvider>());
}

NewTabPageExtensionConfigProvider::NewTabPageExtensionConfigProvider()
    : ExtensionConfigProvider(extension_misc::kNewTabPageExtensionId) {}

NewTabPageExtensionConfigProvider::~NewTabPageExtensionConfigProvider() =
    default;

base::DictValue NewTabPageExtensionConfigProvider::GetLoadTimeData(
    content::BrowserContext& context) {
  Profile* profile = Profile::FromBrowserContext(&context);
  // Files can be dropped into the searchbox only if search content sharing is
  // allowed.
  const bool session_allows_drag_and_drop =
      ContextualSearchServiceFactory::GetForProfile(profile) &&
      contextual_search::ContextualSearchService::IsContextSharingEnabled(
          profile->GetPrefs());
  // The extension reuses chrome://new-tab-page elements, so it gets the same
  // loadTimeData.
  base::DictValue dict =
      NewTabPageUI::GetLoadTimeData(profile, session_allows_drag_and_drop);
  // Adds `fontfamily`, `fontsize`, `language`, `textdirection` etc., which
  // `content::WebUIDataSource` adds on chrome://new-tab-page.
  webui::SetLoadTimeDataDefaults(g_browser_process->GetApplicationLocale(),
                                 &dict);
  return dict;
}

bool NewTabPageExtensionConfigProvider::IsJsErrorReportingEnabled() const {
  return true;
}
