// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PROMOS_IOS_PROMO_CONTROLLER_H_
#define CHROME_BROWSER_UI_PROMOS_IOS_PROMO_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class Browser;
class BrowserWindowInterface;

namespace desktop_to_mobile_promos {
enum class PromoType;
}

// This controller is responsible for showing iOS promos for a specific
// browser window. An instance of this class is created for each window.
// TODO(crbug.com/446944658): This controller is a temporary solution for
// triggering promos. The long-term plan is to migrate the presentation logic
// to the Browser User Education system. Once that is complete, this class and
// its associated dependency cycle workaround in the build files can be removed.
class IOSPromoController {
 public:
  DECLARE_USER_DATA(IOSPromoController);

  explicit IOSPromoController(Browser* browser);
  ~IOSPromoController();

  IOSPromoController(const IOSPromoController&) = delete;
  IOSPromoController& operator=(const IOSPromoController&) = delete;

  static IOSPromoController* From(
      BrowserWindowInterface* browser_window_interface);

 private:
  void OnPromoTriggered(desktop_to_mobile_promos::PromoType promo_type);

  const raw_ptr<Browser> browser_;

  base::CallbackListSubscription promo_trigger_subscription_;

  ui::ScopedUnownedUserData<IOSPromoController> scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_PROMOS_IOS_PROMO_CONTROLLER_H_
