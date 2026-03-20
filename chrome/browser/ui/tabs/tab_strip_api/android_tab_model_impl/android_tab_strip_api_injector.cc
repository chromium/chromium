// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/android_tab_model_impl/android_tab_strip_api_injector.h"

#include "base/notreached.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/event_bridge.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/translation_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/android_tab_model_impl/android_browser_adapter_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_api/android_tab_model_impl/android_tab_strip_model_adapter.h"

namespace tabs_api {

base::expected<mojom::TabPtr, mojo_base::mojom::ErrorPtr>
UselessTranslationAdapter::ToMojoTab(tabs::TabHandle handle) {
  NOTREACHED() << "not implemented";
}

AndroidTabStripApiInjector::AndroidTabStripApiInjector(TabModel* model)
    : browser_adapter_(std::make_unique<AndroidBrowserAdapterImpl>()),
      tab_model_adapter_(std::make_unique<AndroidTabStripModelAdapter>(model)) {

}

AndroidTabStripApiInjector::~AndroidTabStripApiInjector() = default;

BrowserAdapter& AndroidTabStripApiInjector::browser_adapter() {
  return *browser_adapter_;
}

TabStripModelAdapter& AndroidTabStripApiInjector::tab_strip_model_adapter() {
  return *tab_model_adapter_;
}

TranslationAdapter& AndroidTabStripApiInjector::translation_adapter() {
  return translator_;
}

EventBridge& AndroidTabStripApiInjector::event_bridge() {
  return bridge_;
}

}  // namespace tabs_api
