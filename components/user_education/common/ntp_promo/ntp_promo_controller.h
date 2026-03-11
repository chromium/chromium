// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_NTP_PROMO_NTP_PROMO_CONTROLLER_H_
#define COMPONENTS_USER_EDUCATION_COMMON_NTP_PROMO_NTP_PROMO_CONTROLLER_H_

#include <memory>

#include "base/auto_reset.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "components/user_education/common/ntp_promo/ntp_promo_identifier.h"
#include "components/user_education/common/ntp_promo/ntp_promo_specification.h"
#include "components/user_education/common/user_education_context.h"
#include "components/user_education/common/user_education_storage_service.h"

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

// This struct provides a single pending promo, intended for use by the New
// Tab Page.
struct NtpShowablePromos {
  NtpShowablePromos();
  ~NtpShowablePromos();
  NtpShowablePromos(NtpShowablePromos&&) noexcept;
  NtpShowablePromos& operator=(NtpShowablePromos&&) noexcept;

  // Returns true if there is no promo to show.
  bool empty() const { return !promo.has_value(); }

  // The single promo to show, if any.
  std::optional<NtpShowablePromo> promo;
};

// This struct holds the values of controller-specific feature parameters.
// An instance of this struct is passed to the `NtpPromoController` constructor.
struct NtpPromoControllerParams {
  NtpPromoControllerParams();
  ~NtpPromoControllerParams();
  NtpPromoControllerParams(const NtpPromoControllerParams&) noexcept;
  NtpPromoControllerParams& operator=(NtpPromoControllerParams&&) noexcept;

  // The number of sessions a promo may stay in the top spot before being
  // rotated out.
  int max_top_spot_sessions = 0;

  // How long a promo is hidden after being clicked.
  base::TimeDelta clicked_hide_duration;

  // How long all promos are hidden after being snoozed.
  base::TimeDelta promos_snoozed_hide_duration;

  // A list of promo IDs to suppress.
  // TODO(crbug.com/427784414): Hook up this setting.
  std::vector<NtpPromoIdentifier> suppress_list;
};

// Returns controller parameters from Finch-controllable feature params.
NtpPromoControllerParams GetNtpPromoControllerParams();

// Controls display of New Tab Page promos.
class NtpPromoController {
 public:
  NtpPromoController(const NtpPromoController&) = delete;
  virtual ~NtpPromoController();
  void operator=(const NtpPromoController&) = delete;

  NtpPromoController(NtpPromoRegistry& registry,
                     UserEducationStorageService& storage_service,
                     const NtpPromoControllerParams& params);

  // Determines if there is a showable promo. This may return false if
  // promos are snoozed or disabled, or if there are no eligible promos to show.
  virtual bool HasShowablePromo(
      const user_education::UserEducationContextPtr& context);

  // Provides a showable promo, intended to be displayed by the NTP.
  // May update prefs as a side effect.
  //
  // If promos are snoozed or disabled, or there are no eligible promos, an
  // empty struct is returned.
  virtual NtpShowablePromos GenerateShowablePromo(
      const user_education::UserEducationContextPtr& context);

  // Called when a promo is shown by the NTP promo component.
  virtual void OnPromoShown(const NtpPromoIdentifier& eligible_shown);

  // Called in response to an NTP promo activation.
  virtual void OnPromoClicked(
      NtpPromoIdentifier id,
      const user_education::UserEducationContextPtr& context);

  // Sets or resets the snoozed state. Snooze, when set, will last for a fixed
  // period of time.
  virtual void SetAllPromosSnoozed(bool snooze);

  // Sets or resets the disabled state. Disable, when set, will last
  // indefinitely.
  virtual void SetAllPromosDisabled(bool disable);

 private:
  // Internal variation of promo list generation, shared between "has promos"
  // and "make promo lists" logic. When only checking if there are promos to
  // show, the (relatively expensive) ordering logic can be skipped.
  NtpShowablePromos GenerateShowablePromo(
      const user_education::UserEducationContextPtr& context,
      bool apply_ordering);

  // Checks which promo ID (if any) was most recently shown in the top spot.
  // Returns an empty string if there is no recorded top-spot promo.
  NtpPromoIdentifier GetMostRecentTopSpotPromo();

  // Returns whether promos are disabled or snoozed.
  bool ArePromosBlocked() const;

  // Determines whether an individual promo should be shown.
  bool ShouldShowPromo(const NtpPromoIdentifier& id,
                       const NtpPromoData& prefs,
                       NtpPromoSpecification::Eligibility eligibility,
                       const base::Time& now);

  const raw_ref<NtpPromoRegistry> registry_;
  const raw_ref<UserEducationStorageService> storage_service_;
  std::unique_ptr<NtpPromoOrderPolicy> order_policy_;
  const NtpPromoControllerParams params_;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_NTP_PROMO_NTP_PROMO_CONTROLLER_H_
