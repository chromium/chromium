// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_MODEL_IMPL_TAB_STRIP_MODEL_INJECTOR_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_MODEL_IMPL_TAB_STRIP_MODEL_INJECTOR_H_

#include "chrome/browser/ui/tabs/tab_strip_api/adapters/experimental_platform_adapters_provider.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/platform_adapters_provider.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_model_impl/browser_adapter_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_model_impl/tab_context_menu_adapter_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_model_impl/tab_strip_model_adapter_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_model_impl/tab_strip_model_event_bridge.h"

class TabStripModel;
class BrowserWindowInterface;

namespace tabs_api::tab_strip_model {

class Translator : public TranslationAdapter {
 public:
  explicit Translator(TabStripModelAdapterImpl& tab_strip_model_adapter);
  Translator(const Translator&&) = delete;
  Translator operator=(const Translator&) = delete;
  ~Translator() override = default;

  // TranslationAdapter;
  base::expected<mojom::TabPtr, mojo_base::mojom::ErrorPtr> ToMojoTab(
      tabs::TabHandle handle) override;
  base::expected<mojom::DataPtr, mojo_base::mojom::ErrorPtr> ToMojoData(
      tabs::TabCollectionHandle handle) override;

 private:
  raw_ref<TabStripModelAdapterImpl> tab_strip_model_adapter_;
};

class TabStripModelInjector : public PlatformAdaptersProvider {
 public:
  TabStripModelInjector(BrowserWindowInterface* browser_window_interface,
                        TabStripModel* tab_strip_model);
  TabStripModelInjector(const TabStripModelInjector&&) = delete;
  TabStripModelInjector operator=(const TabStripModelInjector&) = delete;
  ~TabStripModelInjector() override;

  // PlatformAdaptersProvider:
  BrowserAdapter& browser_adapter() override;
  TabStripModelAdapter& tab_strip_model_adapter() override;
  TranslationAdapter& translation_adapter() override;
  EventBridge& event_bridge() override;

 private:
  std::unique_ptr<TabStripModelAdapterImpl> tab_strip_model_adapter_;
  std::unique_ptr<BrowserAdapterImpl> browser_adapter_;
  Translator translator_;
  TabStripModelEventBridge event_bridge_;
};

class TabStripModelExperimentalInjector
    : public ExperimentalPlatformAdaptersProvider {
 public:
  explicit TabStripModelExperimentalInjector(
      BrowserWindowInterface* browser_window_interface);
  TabStripModelExperimentalInjector(const TabStripModelExperimentalInjector&&) =
      delete;
  TabStripModelExperimentalInjector operator=(
      const TabStripModelExperimentalInjector&) = delete;
  ~TabStripModelExperimentalInjector() override;

  // ExperimentalPlatformAdaptersProvider:
  ContextMenuAdapter& context_menu_adapter() override;

 private:
  std::unique_ptr<TabContextMenuAdapterImpl> context_menu_adapter_;
};

}  // namespace tabs_api::tab_strip_model

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_MODEL_IMPL_TAB_STRIP_MODEL_INJECTOR_H_
