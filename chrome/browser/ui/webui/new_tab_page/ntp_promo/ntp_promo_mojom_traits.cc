// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/ntp_promo/ntp_promo_mojom_traits.h"

#include "base/notreached.h"
#include "chrome/browser/ui/webui/new_tab_page/ntp_promo/ntp_promo.mojom-data-view.h"
#include "chrome/browser/ui/webui/new_tab_page/ntp_promo/ntp_promo.mojom.h"
#include "components/user_education/common/ntp_promo/ntp_promo_controller.h"

namespace mojo {

bool StructTraits<ntp_promo::mojom::PromoDataView,
                  user_education::NtpShowablePromo>::
    Read(ntp_promo::mojom::PromoDataView promo,
         user_education::NtpShowablePromo* showable) {
  return promo.ReadId(&showable->id) &&
         promo.ReadIconName(&showable->icon_name) &&
         promo.ReadBodyText(&showable->body_text) &&
         promo.ReadButtonText(&showable->action_button_text);
}

}  // namespace mojo
