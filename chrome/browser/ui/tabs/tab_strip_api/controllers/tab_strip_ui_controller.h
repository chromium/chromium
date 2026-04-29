// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_CONTROLLERS_TAB_STRIP_UI_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_CONTROLLERS_TAB_STRIP_UI_CONTROLLER_H_

#include "components/browser_apis/tab_strip/tab_strip_ui_controller.mojom.h"

namespace tabs_api {

class BrowserAdapter;
class ContextMenuAdapter;
class TabStripModelAdapter;

class TabStripUIControllerInjector {
 public:
  virtual ~TabStripUIControllerInjector() = default;

  virtual BrowserAdapter& browser_adapter() = 0;
  virtual TabStripModelAdapter& tab_strip_model_adapter() = 0;
  virtual ContextMenuAdapter& context_menu_adapter() = 0;
};

class TabStripUIController
    : public mojom::TabStripUIControllerDirectReturnStub {
 public:
  ~TabStripUIController() override = default;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_CONTROLLERS_TAB_STRIP_UI_CONTROLLER_H_
