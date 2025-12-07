// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NTP_PROMO_NTP_PROMO_MOJOM_TRAITS_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NTP_PROMO_NTP_PROMO_MOJOM_TRAITS_H_

#include "chrome/browser/ui/webui/new_tab_page/ntp_promo/ntp_promo.mojom-data-view.h"
#include "chrome/browser/ui/webui/new_tab_page/ntp_promo/ntp_promo.mojom.h"
#include "components/user_education/common/ntp_promo/ntp_promo_controller.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<ntp_promo::mojom::PromoDataView,
                    user_education::NtpShowablePromo> {
  static std::string id(const user_education::NtpShowablePromo& promo) {
    return promo.id;
  }
  static std::string icon_name(const user_education::NtpShowablePromo& promo) {
    return promo.icon_name;
  }
  static std::string body_text(const user_education::NtpShowablePromo& promo) {
    return promo.body_text;
  }
  static std::string button_text(
      const user_education::NtpShowablePromo& promo) {
    return promo.action_button_text;
  }

  static bool Read(ntp_promo::mojom::PromoDataView promo,
                   user_education::NtpShowablePromo* showable);
};

}  // namespace mojo

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NTP_PROMO_NTP_PROMO_MOJOM_TRAITS_H_
