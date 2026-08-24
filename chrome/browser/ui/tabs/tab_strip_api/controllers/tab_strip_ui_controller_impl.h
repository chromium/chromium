// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_CONTROLLERS_TAB_STRIP_UI_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_CONTROLLERS_TAB_STRIP_UI_CONTROLLER_IMPL_H_

#include <memory>

#include "chrome/browser/ui/tabs/tab_strip_api/controllers/tab_strip_ui_controller.h"
#include "components/browser_apis/tab_strip/tab_strip_ui_controller.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

namespace tabs_api {

class TabStripUIControllerImpl : public TabStripUIController {
 public:
  DECLARE_USER_DATA(TabStripUIControllerImpl);

  // `host` is the UnownedUserDataHost of the browser window this controller
  // serves; tests may pass their own host.
  TabStripUIControllerImpl(
      std::unique_ptr<TabStripUIControllerInjector> injector,
      ui::UnownedUserDataHost& host);
  TabStripUIControllerImpl(const TabStripUIControllerImpl&) = delete;
  TabStripUIControllerImpl& operator=(const TabStripUIControllerImpl&) = delete;
  ~TabStripUIControllerImpl() override;

  // Returns the controller for `browser_window`, or null if it does not
  // have one.
  static TabStripUIControllerImpl* From(BrowserWindowInterface* browser_window);

  void Bind(mojo::PendingReceiver<mojom::TabStripUIController> receiver);

  // mojom::TabStripUIControllerDirectReturnStub:
  mojom::TabStripUIController::ShowTabContextMenuResult ShowTabContextMenu(
      const tabs_api::NodeId& id,
      const gfx::Point& location) override;

 private:
  std::unique_ptr<TabStripUIControllerInjector> injector_;

  mojom::TabStripUIControllerBridge bridge_{this};
  mojo::ReceiverSet<mojom::TabStripUIController> receivers_;

  ui::ScopedUnownedUserData<TabStripUIControllerImpl> scoped_unowned_user_data_;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_CONTROLLERS_TAB_STRIP_UI_CONTROLLER_IMPL_H_
