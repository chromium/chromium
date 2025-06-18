// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/ntp_promo/ntp_promo_registry.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "components/user_education/common/ntp_promo/ntp_promo_specification.h"

namespace user_education {

NtpPromoRegistry::NtpPromoRegistry() = default;
NtpPromoRegistry::~NtpPromoRegistry() = default;

const NtpPromoSpecification* NtpPromoRegistry::GetNtpPromoSpecification(
    const NtpPromoIdentifier& id) const {
  auto pair = ntp_promo_registry_.find(id);
  if (pair == ntp_promo_registry_.end()) {
    return nullptr;
  }
  return &pair->second;
}

std::vector<NtpPromoIdentifier> NtpPromoRegistry::GetNtpPromoIdentifiers()
    const {
  std::vector<NtpPromoIdentifier> id_strings;
  std::ranges::transform(ntp_promo_registry_, std::back_inserter(id_strings),
                         &Registry::value_type::first);
  return id_strings;
}

void NtpPromoRegistry::AddPromo(NtpPromoSpecification specification) {
  auto [it, inserted] =
      ntp_promo_registry_.emplace(specification.id(), std::move(specification));
  CHECK(inserted);
}

}  // namespace user_education
