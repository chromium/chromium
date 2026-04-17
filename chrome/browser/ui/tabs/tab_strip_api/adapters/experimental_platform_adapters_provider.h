// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_EXPERIMENTAL_PLATFORM_ADAPTERS_PROVIDER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_EXPERIMENTAL_PLATFORM_ADAPTERS_PROVIDER_H_

namespace tabs_api {

class ContextMenuAdapter;

// Configures the tab strip service for an underlying tab strip model.
//
// This injector is intended for quick prototyping for experimental apis that
// may not necessarily fit in the standard TabStripService. Some models may not
// implement all interfaces.
class ExperimentalPlatformAdaptersProvider {
 public:
  virtual ContextMenuAdapter& context_menu_adapter() = 0;

  virtual ~ExperimentalPlatformAdaptersProvider() = default;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_EXPERIMENTAL_PLATFORM_ADAPTERS_PROVIDER_H_
