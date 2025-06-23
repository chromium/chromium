// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_NTP_PROMO_NTP_PROMO_CONTROLLER_H_
#define COMPONENTS_USER_EDUCATION_COMMON_NTP_PROMO_NTP_PROMO_CONTROLLER_H_

#include <memory>

#include "base/auto_reset.h"
#include "base/feature_list.h"
#include "base/memory/raw_ref.h"
#include "components/user_education/common/feature_promo/feature_promo_registry.h"
#include "components/user_education/common/ntp_promo/ntp_promo_registry.h"
#include "components/user_education/common/ntp_promo/ntp_promo_specification.h"
#include "components/user_education/common/user_education_data.h"
#include "components/user_education/common/user_education_storage_service.h"
#include "ui/menus/simple_menu_model.h"

namespace user_education {

// This struct provides ordered sets of pending and completed promos, intended
// for use by the New Tab Page.
struct NtpShowablePromos {
  NtpShowablePromos();
  ~NtpShowablePromos();
  NtpShowablePromos(NtpShowablePromos&&) noexcept;
  NtpShowablePromos& operator=(NtpShowablePromos&&) noexcept;

  struct Promo {
    Promo(NtpPromoIdentifier id, const NtpPromoContent& content);

    std::string id;
    NtpPromoContent content;
  };

  // Lists of promos, in descending priority order. Ie, if the UI chooses to
  // show only one promo from a list, choose the first one.
  std::vector<Promo> pending;
  std::vector<Promo> completed;
};

// Controls display of New Tab Page promos.
class NtpPromoController {
 public:
  NtpPromoController(const NtpPromoController&) = delete;
  ~NtpPromoController();
  void operator=(const NtpPromoController&) = delete;

  NtpPromoController(NtpPromoRegistry& registry,
                     UserEducationStorageService& storage_service);

  // Provides ordered lists of eligible and completed promos, intended to be
  // displayed by the NTP.
  NtpShowablePromos GetShowablePromos();

  // Called in response to an NTP promo activation.
  void OnPromoClicked(NtpPromoIdentifier id);

 private:
  const raw_ref<NtpPromoRegistry> registry_;
  const raw_ref<UserEducationStorageService> storage_service_;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_NTP_PROMO_NTP_PROMO_CONTROLLER_H_
