// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/controllers/tab_strip_ui_controller_injector_impl.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_model_impl/browser_adapter_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_model_impl/tab_context_menu_adapter_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_model_impl/tab_strip_model_adapter_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

namespace tabs_api {

TabStripUIControllerInjectorImpl::TabStripUIControllerInjectorImpl(
    BrowserWindowInterface* browser,
    TabStripModel* tab_strip_model)
    : tab_strip_model_adapter_(std::make_unique<TabStripModelAdapterImpl>(
          tab_strip_model,
          base::NumberToString(browser->GetSessionID().id()))),
      browser_adapter_(std::make_unique<BrowserAdapterImpl>(browser)),
      context_menu_adapter_(
          std::make_unique<TabContextMenuAdapterImpl>(browser,
                                                      tab_strip_model)) {}

TabStripUIControllerInjectorImpl::~TabStripUIControllerInjectorImpl() = default;

BrowserAdapter& TabStripUIControllerInjectorImpl::browser_adapter() {
  return *browser_adapter_;
}

TabStripModelAdapter&
TabStripUIControllerInjectorImpl::tab_strip_model_adapter() {
  return *tab_strip_model_adapter_;
}

ContextMenuAdapter& TabStripUIControllerInjectorImpl::context_menu_adapter() {
  return *context_menu_adapter_;
}

}  // namespace tabs_api
