// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/testing/injector.h"

namespace tabs_api::testing {

Injector::Injector(ToyTabStrip& toy_tab_strip)
    : browser_adapter_(
          std::make_unique<ToyTabStripBrowserAdapter>(&toy_tab_strip)),
      tab_strip_model_adapter_(
          std::make_unique<ToyTabStripModelAdapter>(&toy_tab_strip)),
      translation_adapter_(
          std::make_unique<ToyTabStripTranslationAdapter>(&toy_tab_strip)),
      event_bridge_(std::make_unique<ToyTabStripEventBridge>(&toy_tab_strip)) {}

Injector::~Injector() = default;

}  // namespace tabs_api::testing
