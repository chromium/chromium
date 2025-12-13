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

// This struct provides ordered sets of pending and completed promos, intended
// for use by the New Tab Page.
struct NtpShowablePromos {
  NtpShowablePromos();
  ~NtpShowablePromos();
  NtpShowablePromos(NtpShowablePromos&&) noexcept;
  NtpShowablePromos& operator=(NtpShowablePromos&&) noexcept;

  // Returns true if there are no promos to show.
  bool empty() const { return pending.empty() && completed.empty(); }

  // Lists of promos, in descending priority order. Ie, if the UI chooses to
  // show only one promo from a list, choose the first one.
  std::vector<NtpShowablePromo> pending;
  std::vector<NtpShowablePromo> completed;
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

  // How long a promo stays in the "completed" section of the setup list.
  base::TimeDelta completed_show_duration;

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

  // Determines if there are any showable promos. This may return false if
  // promos are snoozed or disabled, or if there are no eligible promos to show.
  virtual bool HasShowablePromos(
      const user_education::UserEducationContextPtr& context,
      bool include_completed);

  // Provides ordered lists of eligible and completed promos, intended to be
  // displayed by the NTP. May update prefs as a side effect.
  //
  // If promos are snoozed or disabled, or there are no eligible promos, an
  // empty list is returned.
  virtual NtpShowablePromos GenerateShowablePromos(
      const user_education::UserEducationContextPtr& context);

  // Called when promos are shown by the NTP promo component.
  //
  // The promos should be ordered in each list from top/first to bottom/last.
  virtual void OnPromosShown(
      const std::vector<NtpPromoIdentifier>& eligible_shown,
      const std::vector<NtpPromoIdentifier>& completed_shown);

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
  NtpShowablePromos GenerateShowablePromos(
      const user_education::UserEducationContextPtr& context,
      bool apply_ordering);

  // Updates the data on the promo shown in the top spot.
  void OnPromoShownInTopSpot(NtpPromoIdentifier id);

  // Checks which promo ID (if any) was most recently shown in the top spot.
  // Returns an empty string if there is no recorded top-spot promo.
  NtpPromoIdentifier GetMostRecentTopSpotPromo();

  // Returns whether promos are disabled or snoozed.
  bool ArePromosBlocked() const;

  // Assembles a vector of showable promo objects (ie. the presentation parts
  // of the promo) to be sent to the NTP.
  std::vector<NtpShowablePromo> MakeShowablePromos(
      const std::vector<NtpPromoIdentifier>& ids);

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
