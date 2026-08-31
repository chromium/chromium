// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_IPH_CONTROLLER_H_
#define CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_IPH_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/send_tab_to_self/send_tab_to_self_model_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

namespace send_tab_to_self {

class SendTabToSelfModel;

// Controller responsible for showing the Send Tab To Self Tutorial IPH promo
// on the first eligible tab opened in a browser window.
class SendTabToSelfIphController : public TabStripModelObserver,
                                   public SendTabToSelfModelObserver {
 public:
  DECLARE_USER_DATA(SendTabToSelfIphController);
  explicit SendTabToSelfIphController(BrowserWindowInterface* interface);
  ~SendTabToSelfIphController() override;

  SendTabToSelfIphController(const SendTabToSelfIphController&) = delete;
  SendTabToSelfIphController& operator=(const SendTabToSelfIphController&) =
      delete;

  static SendTabToSelfIphController* From(BrowserWindowInterface* interface);

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void OnTabChangedAt(tabs::TabInterface* tab,
                      TabChangeType change_type) override;

  // SendTabToSelfModelObserver:
  void OnModelReady() override;

 private:
  void MaybeShowPromo();

  raw_ptr<BrowserWindowInterface> browser_window_interface_;
  ui::ScopedUnownedUserData<SendTabToSelfIphController> scoped_data_;
  base::ScopedObservation<SendTabToSelfModel, SendTabToSelfModelObserver>
      model_observation_{this};
  bool promo_shown_ = false;
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_IPH_CONTROLLER_H_
