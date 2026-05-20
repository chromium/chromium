// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKTOP_TO_MOBILE_PROMOS_DESKTOP_TO_MOBILE_PROMO_UTILS_H_
#define COMPONENTS_DESKTOP_TO_MOBILE_PROMOS_DESKTOP_TO_MOBILE_PROMO_UTILS_H_

#include "components/desktop_to_mobile_promos/promos_types.h"

namespace desktop_to_mobile_promos {

// Holds the string resource IDs for forced or production promo push
// notifications.
struct PromoNotificationStringIDs {
  int title_id = 0;
  int body_id = 0;
};

// Returns the corresponding title and body string resource IDs for a PromoType.
// Returns zero-initialized IDs if the promo type is invalid or unsupported.
PromoNotificationStringIDs GetPromoNotificationStringIDs(PromoType promo_type);

}  // namespace desktop_to_mobile_promos

#endif  // COMPONENTS_DESKTOP_TO_MOBILE_PROMOS_DESKTOP_TO_MOBILE_PROMO_UTILS_H_
