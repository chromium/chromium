// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_NTP_PROMO_NTP_PROMO_REGISTRY_H_
#define COMPONENTS_USER_EDUCATION_COMMON_NTP_PROMO_NTP_PROMO_REGISTRY_H_

#include "components/user_education/common/ntp_promo/ntp_promo_specification.h"

namespace user_education {

// An NtpPromoRegistry is a Map of NtpPromoIdentifier to NtpPromoSpecifications.
class NtpPromoRegistry {
 public:
  NtpPromoRegistry();
  ~NtpPromoRegistry();
  NtpPromoRegistry(const NtpPromoRegistry&) = delete;
  NtpPromoRegistry& operator=(const NtpPromoRegistry&) = delete;

  // Returns a list of registered NtpPromoIdentifiers.
  const std::vector<NtpPromoIdentifier>& GetNtpPromoIdentifiers() const;

  // Gets the requested NtpPromoSpecification from the registry, or nullptr
  // if the promo is not registered.
  const NtpPromoSpecification* GetNtpPromoSpecification(
      const NtpPromoIdentifier& id) const;

  // Adds an NtpPromoSpecification to the registry.
  void AddPromo(NtpPromoSpecification spec);

  bool AreAnyPromosRegistered() const;

  void ClearPromoForTesting(const NtpPromoIdentifier& id);
  void ClearPromosForTesting();

 private:
  using Registry = std::map<NtpPromoIdentifier, NtpPromoSpecification>;

  // Keep both a map of ID to specification, and a vector of IDs. This gives
  // easy lookups while preserving promo registration order.
  Registry registry_;
  std::vector<NtpPromoIdentifier> identifiers_;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_NTP_PROMO_NTP_PROMO_REGISTRY_H_
