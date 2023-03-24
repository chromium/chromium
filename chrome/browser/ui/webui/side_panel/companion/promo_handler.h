// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_COMPANION_PROMO_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_COMPANION_PROMO_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/side_panel/companion/companion.mojom.h"

class PrefService;

namespace companion {
using side_panel::mojom::PromoAction;
using side_panel::mojom::PromoType;

// Central class to handle user actions on various promos displayed in the
// search companion.
class PromoHandler {
 public:
  explicit PromoHandler(PrefService* pref_service);
  PromoHandler(const PromoHandler&) = delete;
  PromoHandler& operator=(const PromoHandler&) = delete;
  ~PromoHandler();

  // Called in response to the mojo call from renderer. Takes necessary action
  // to handle the user action on the promo.
  void OnPromoAction(PromoType promo_type, PromoAction promo_action);

 private:
  // Lifetime of the PrefService is bound to profile which outlives the lifetime
  // of the companion page.
  raw_ptr<PrefService> pref_service_;
};

}  // namespace companion

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_COMPANION_PROMO_HANDLER_H_
