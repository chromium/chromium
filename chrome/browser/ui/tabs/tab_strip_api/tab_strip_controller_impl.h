// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_CONTROLLER_IMPL_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api.mojom.h"

class BrowserWindowInterface;
class TabStripModel;

// TODO (crbug.com/409086859). See bug for dd.
// tabs_api::mojom::TabStripController is an experimental TabStrip Api between
// any view and the TabStripModel.
class TabStripControllerImpl : public tabs_api::mojom::TabStripController {
 public:
  explicit TabStripControllerImpl(BrowserWindowInterface* browser,
                                  TabStripModel* tab_strip_model);
  TabStripControllerImpl(const TabStripControllerImpl&) = delete;
  TabStripControllerImpl& operator=(const TabStripControllerImpl&) = delete;
  ~TabStripControllerImpl() override;

  // tabs_api::mojom::TabStripController overrides
  void CreateNewTab(CreateNewTabCallback callback) override;

 private:
  raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<TabStripModel> model_;
};
#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_CONTROLLER_IMPL_H_
