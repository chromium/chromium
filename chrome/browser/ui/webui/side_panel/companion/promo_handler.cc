// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/companion/promo_handler.h"

#include "components/prefs/pref_service.h"

namespace companion {

PromoHandler::PromoHandler(PrefService* pref_service)
    : pref_service_(pref_service) {}

PromoHandler::~PromoHandler() = default;

void PromoHandler::OnPromoAction(PromoType promo_type,
                                 PromoAction promo_action) {
  // TODO(b/273652233): Implement logic to persist promo count, and optionally
  // open sign-in flow.
}

}  // namespace companion
