// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_NTP_PROMO_NTP_PROMO_CONTROLLER_H_
#define COMPONENTS_USER_EDUCATION_COMMON_NTP_PROMO_NTP_PROMO_CONTROLLER_H_

#include <memory>

#include "base/auto_reset.h"
#include "base/feature_list.h"
#include "base/memory/raw_ref.h"
#include "components/user_education/common/ntp_promo/ntp_promo_registry.h"
#include "components/user_education/common/ntp_promo/ntp_promo_specification.h"
#include "components/user_education/common/user_education_data.h"
#include "components/user_education/common/user_education_storage_service.h"

namespace user_education {

// The contents of a promo as it will be shown in the NTP.
struct NtpShowablePromo {
  NtpShowablePromo();
  NtpShowablePromo(std::string_view id_,
                   std::string_view icon_name_,
                   std::string_view body_text_,
                   std::string_view action_button_text_);
  NtpShowablePromo(const NtpShowablePromo& other);
  NtpShowablePromo& operator=(const NtpShowablePromo& other);
  ~NtpShowablePromo();

  std::string id;
  std::string icon_name;
  std::string body_text;
  std::string action_button_text;
};

// This struct provides ordered sets of pending and completed promos, intended
// for use by the New Tab Page.
struct NtpShowablePromos {
  NtpShowablePromos();
  ~NtpShowablePromos();
  NtpShowablePromos(NtpShowablePromos&&) noexcept;
  NtpShowablePromos& operator=(NtpShowablePromos&&) noexcept;

  // Lists of promos, in descending priority order. Ie, if the UI chooses to
  // show only one promo from a list, choose the first one.
  std::vector<NtpShowablePromo> pending;
  std::vector<NtpShowablePromo> completed;
};

// Controls display of New Tab Page promos.
class NtpPromoController {
 public:
  NtpPromoController(const NtpPromoController&) = delete;
  virtual ~NtpPromoController();
  void operator=(const NtpPromoController&) = delete;

  NtpPromoController(NtpPromoRegistry& registry,
                     UserEducationStorageService& storage_service);

  // Provides ordered lists of eligible and completed promos, intended to be
  // displayed by the NTP. May update prefs as a side effect.
  virtual NtpShowablePromos GenerateShowablePromos();

  // Called in response to an NTP promo activation.
  virtual void OnPromoClicked(NtpPromoIdentifier id);

  // Returns the duration for which a promo can be shown after completion.
  static base::TimeDelta GetCompletedPromoShowDurationForTest();

 private:
  const raw_ref<NtpPromoRegistry> registry_;
  const raw_ref<UserEducationStorageService> storage_service_;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_NTP_PROMO_NTP_PROMO_CONTROLLER_H_
