// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_EXTENSION_NEW_TAB_PAGE_EXTENSION_CONFIG_PROVIDER_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_EXTENSION_NEW_TAB_PAGE_EXTENSION_CONFIG_PROVIDER_H_

#include "base/values.h"
#include "extensions/browser/extension_config_map.h"

namespace content {
class BrowserContext;
}  // namespace content

// Provides configuration for the NTP component extension: `loadTimeData` and
// `$i18n{}` strings, which are the same as chrome://new-tab-page's.
class NewTabPageExtensionConfigProvider
    : public extensions::ExtensionConfigProvider {
 public:
  static void Register(content::BrowserContext* browser_context);

  NewTabPageExtensionConfigProvider();
  ~NewTabPageExtensionConfigProvider() override;

  // extensions::ExtensionConfigProvider:
  base::DictValue GetLoadTimeData(content::BrowserContext& context) override;
  bool IsJsErrorReportingEnabled() const override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_EXTENSION_NEW_TAB_PAGE_EXTENSION_CONFIG_PROVIDER_H_
