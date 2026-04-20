// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_model_impl/tab_strip_model_injector.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_model_impl/converters/tab_converters.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

namespace tabs_api::tab_strip_model {

Translator::Translator(TabStripModelAdapterImpl& tab_strip_model_adapter)
    : tab_strip_model_adapter_(tab_strip_model_adapter) {}

base::expected<mojom::TabPtr, mojo_base::mojom::ErrorPtr> Translator::ToMojoTab(
    tabs::TabHandle tab_handle) {
  const ui::ColorProvider& color_provider =
      tab_strip_model_adapter_->GetColorProvider();
  auto mojo_tab = tabs_api::converters::BuildMojoTab(
      tab_handle.Get(), color_provider,
      tab_strip_model_adapter_->GetTabStates(tab_handle));
  return std::move(mojo_tab);
}

base::expected<mojom::DataPtr, mojo_base::mojom::ErrorPtr>
Translator::ToMojoData(tabs::TabCollectionHandle handle) {
  return tabs_api::converters::BuildMojoTabCollectionData(handle);
}

TabStripModelInjector::TabStripModelInjector(
    BrowserWindowInterface* browser_window_interface,
    TabStripModel* tab_strip_model)
    : tab_strip_model_adapter_(std::make_unique<TabStripModelAdapterImpl>(
          tab_strip_model,
          base::NumberToString(browser_window_interface->GetSessionID().id()))),
      browser_adapter_(
          std::make_unique<BrowserAdapterImpl>(browser_window_interface)),
      translator_(*tab_strip_model_adapter_),
      event_bridge_(*tab_strip_model_adapter_) {}

TabStripModelInjector::~TabStripModelInjector() = default;

BrowserAdapter& TabStripModelInjector::browser_adapter() {
  return *browser_adapter_;
}

TabStripModelAdapter& TabStripModelInjector::tab_strip_model_adapter() {
  return *tab_strip_model_adapter_;
}

TranslationAdapter& TabStripModelInjector::translation_adapter() {
  return translator_;
}

EventBridge& TabStripModelInjector::event_bridge() {
  return event_bridge_;
}

TabStripModelExperimentalInjector::TabStripModelExperimentalInjector(
    BrowserWindowInterface* browser_window_interface,
    TabStripModel* tab_strip_model)
    : context_menu_adapter_(
          std::make_unique<TabContextMenuAdapterImpl>(browser_window_interface,
                                                      tab_strip_model)) {}

TabStripModelExperimentalInjector::~TabStripModelExperimentalInjector() =
    default;

ContextMenuAdapter& TabStripModelExperimentalInjector::context_menu_adapter() {
  return *context_menu_adapter_;
}

}  // namespace tabs_api::tab_strip_model
