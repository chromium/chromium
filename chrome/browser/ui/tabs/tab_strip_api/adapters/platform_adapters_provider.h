// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_PLATFORM_ADAPTERS_PROVIDER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_PLATFORM_ADAPTERS_PROVIDER_H_

#include "chrome/browser/ui/tabs/tab_strip_api/adapters/browser_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/event_bridge.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tab_strip_model_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/translation_adapter.h"

namespace tabs_api {

// Configures the tab strip service for an underlying tab strip model. The model
// must implement all interfaces. The interfaces themselves are platform
// agnostic.
class PlatformAdaptersProvider {
 public:
  virtual BrowserAdapter& browser_adapter() = 0;
  virtual TabStripModelAdapter& tab_strip_model_adapter() = 0;
  virtual TranslationAdapter& translation_adapter() = 0;
  virtual EventBridge& event_bridge() = 0;

  virtual ~PlatformAdaptersProvider() = default;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_PLATFORM_ADAPTERS_PROVIDER_H_
