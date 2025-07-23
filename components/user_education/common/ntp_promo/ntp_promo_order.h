// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_NTP_PROMO_NTP_PROMO_ORDER_H_
#define COMPONENTS_USER_EDUCATION_COMMON_NTP_PROMO_NTP_PROMO_ORDER_H_

#include <vector>

#include "base/memory/raw_ref.h"
#include "components/user_education/common/ntp_promo/ntp_promo_identifier.h"

namespace user_education {

class NtpPromoRegistry;
class UserEducationStorageService;

// This object sorts eligible (showable) promos into a order, from which the
// New Tab Page can select one or more to show. The implementation manages such
// things as having a particular promo reside at the top of the list for a
// particular number of sessions before rotating out of the top spot.
class NtpPromoOrderPolicy {
 public:
  NtpPromoOrderPolicy() = delete;
  NtpPromoOrderPolicy(const NtpPromoRegistry& registry,
                      const UserEducationStorageService& storage_service,
                      int num_sessions_between_rotation);

  ~NtpPromoOrderPolicy();
  NtpPromoOrderPolicy(const NtpPromoOrderPolicy&) = delete;
  NtpPromoOrderPolicy& operator=(const NtpPromoOrderPolicy&) = delete;

  // Apply ordering to the supplied pending promo IDs, returning a new,
  // ordered vector.
  std::vector<NtpPromoIdentifier> OrderPendingPromos(
      const std::vector<NtpPromoIdentifier>& ids);

  std::vector<NtpPromoIdentifier> OrderCompletedPromos(
      const std::vector<NtpPromoIdentifier>& ids);

 private:
  const raw_ref<const NtpPromoRegistry> registry_;
  const raw_ref<const UserEducationStorageService> storage_service_;

  // Used to control how long a particular promo stays in the top spot, before
  // the next promo is rotated into that spot. If 0, no rotation is done.
  int num_sessions_between_rotation_ = 0;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_NTP_PROMO_NTP_PROMO_ORDER_H_
