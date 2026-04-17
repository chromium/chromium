// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TESTING_EXPERIMENTAL_INJECTOR_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TESTING_EXPERIMENTAL_INJECTOR_H_

#include <memory>

#include "chrome/browser/ui/tabs/tab_strip_api/adapters/experimental_platform_adapters_provider.h"
#include "chrome/browser/ui/tabs/tab_strip_api/testing/toy_tab_context_menu_adapter.h"

namespace tabs_api::testing {

class ExperimentalInjector : public ExperimentalPlatformAdaptersProvider {
 public:
  ExperimentalInjector();
  ~ExperimentalInjector() override;

  // ExperimentalPlatformAdaptersProvider:
  ContextMenuAdapter& context_menu_adapter() override {
    return *context_menu_adapter_;
  }

 private:
  std::unique_ptr<ToyTabContextMenuAdapter> context_menu_adapter_;
};

}  // namespace tabs_api::testing

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TESTING_EXPERIMENTAL_INJECTOR_H_
