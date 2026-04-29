// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_CONTROLLERS_TAB_STRIP_UI_CONTROLLER_INJECTOR_IMPL_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_CONTROLLERS_TAB_STRIP_UI_CONTROLLER_INJECTOR_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/browser_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/context_menu_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tab_strip_model_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/controllers/tab_strip_ui_controller.h"

class BrowserWindowInterface;
class TabStripModel;

namespace tabs_api {

class BrowserAdapterImpl;
class TabContextMenuAdapterImpl;
class TabStripModelAdapterImpl;

class TabStripUIControllerInjectorImpl : public TabStripUIControllerInjector {
 public:
  TabStripUIControllerInjectorImpl(BrowserWindowInterface* browser,
                                   TabStripModel* tab_strip_model);

  ~TabStripUIControllerInjectorImpl() override;

  BrowserAdapter& browser_adapter() override;
  TabStripModelAdapter& tab_strip_model_adapter() override;
  ContextMenuAdapter& context_menu_adapter() override;

 private:
  std::unique_ptr<TabStripModelAdapterImpl> tab_strip_model_adapter_;
  std::unique_ptr<BrowserAdapterImpl> browser_adapter_;
  std::unique_ptr<TabContextMenuAdapterImpl> context_menu_adapter_;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_CONTROLLERS_TAB_STRIP_UI_CONTROLLER_INJECTOR_IMPL_H_
