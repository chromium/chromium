// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TESTING_INJECTOR_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TESTING_INJECTOR_H_

#include "chrome/browser/ui/tabs/tab_strip_api/adapters/platform_adapters_provider.h"
#include "chrome/browser/ui/tabs/tab_strip_api/testing/toy_tab_strip_browser_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/testing/toy_tab_strip_event_bridge.h"
#include "chrome/browser/ui/tabs/tab_strip_api/testing/toy_tab_strip_model_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/testing/toy_tab_strip_translation_adapter.h"

namespace tabs_api::testing {

class Injector : public PlatformAdaptersProvider {
 public:
  explicit Injector(ToyTabStrip& toy_tab_strip);
  ~Injector() override;

  // PlatformAdaptersProvider:
  BrowserAdapter& browser_adapter() override { return *browser_adapter_; }

  TabStripModelAdapter& tab_strip_model_adapter() override {
    return *tab_strip_model_adapter_;
  }

  TranslationAdapter& translation_adapter() override {
    return *translation_adapter_;
  }

  EventBridge& event_bridge() override { return *event_bridge_; }

 private:
  std::unique_ptr<ToyTabStripBrowserAdapter> browser_adapter_;
  std::unique_ptr<ToyTabStripModelAdapter> tab_strip_model_adapter_;
  std::unique_ptr<ToyTabStripTranslationAdapter> translation_adapter_;
  std::unique_ptr<ToyTabStripEventBridge> event_bridge_;
};

}  // namespace tabs_api::testing

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TESTING_INJECTOR_H_
