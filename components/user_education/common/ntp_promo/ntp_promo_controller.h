// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_NTP_PROMO_NTP_PROMO_CONTROLLER_H_
#define COMPONENTS_USER_EDUCATION_COMMON_NTP_PROMO_NTP_PROMO_CONTROLLER_H_

#include <memory>

#include "base/auto_reset.h"
#include "base/feature_list.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "components/user_education/common/ntp_promo/ntp_promo_identifier.h"

class BrowserWindowInterface;
class Profile;

namespace user_education {

class NtpPromoRegistry;
class NtpPromoOrderPolicy;
class UserEducationStorageService;

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

  // Determines if there are any showable proms.
  virtual bool HasShowablePromos(Profile* profile) const;

  // Provides ordered lists of eligible and completed promos, intended to be
  // displayed by the NTP. May update prefs as a side effect.
  virtual NtpShowablePromos GenerateShowablePromos(Profile* profile);

  // Called when promos are shown by the NTP promo component.
  //
  // The promos should be ordered in each list from top/first to bottom/last.
  virtual void OnPromosShown(
      const std::vector<NtpPromoIdentifier>& eligible_shown,
      const std::vector<NtpPromoIdentifier>& completed_shown);

  // Called in response to an NTP promo activation.
  virtual void OnPromoClicked(NtpPromoIdentifier id,
                              BrowserWindowInterface* browser);

  // Returns the duration for which a promo can be shown after completion.
  static base::TimeDelta GetCompletedPromoShowDurationForTest();

  // Returns the duration for which a promo will be hidden after being clicked.
  static base::TimeDelta GetClickedPromoHideDurationForTest();

 private:
  // Updates the data on the promo shown in the top spot.
  void OnPromoShownInTopSpot(NtpPromoIdentifier id);

  // Checks which promo ID (if any) was most recently shown in the top spot.
  // Returns an empty string if there is no recorded top-spot promo.
  NtpPromoIdentifier GetMostRecentTopSpotPromo();

  // Assembles a vector of showable promo objects (ie. the presentation parts
  // of the promo) to be sent to the NTP.
  std::vector<NtpShowablePromo> MakeShowablePromos(
      const std::vector<NtpPromoIdentifier>& ids);

  const raw_ref<NtpPromoRegistry> registry_;
  const raw_ref<UserEducationStorageService> storage_service_;
  std::unique_ptr<NtpPromoOrderPolicy> order_policy_;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_NTP_PROMO_NTP_PROMO_CONTROLLER_H_
