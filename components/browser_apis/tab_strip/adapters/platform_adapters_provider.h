// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_STRIP_ADAPTERS_PLATFORM_ADAPTERS_PROVIDER_H_
#define COMPONENTS_BROWSER_APIS_TAB_STRIP_ADAPTERS_PLATFORM_ADAPTERS_PROVIDER_H_

#include "components/browser_apis/tab_strip/adapters/browser_adapter.h"
#include "components/browser_apis/tab_strip/adapters/event_bridge.h"
#include "components/browser_apis/tab_strip/adapters/tab_strip_model_adapter.h"
#include "components/browser_apis/tab_strip/adapters/translation_adapter.h"

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

#endif  // COMPONENTS_BROWSER_APIS_TAB_STRIP_ADAPTERS_PLATFORM_ADAPTERS_PROVIDER_H_
