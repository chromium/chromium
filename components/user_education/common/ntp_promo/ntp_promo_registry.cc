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
  auto pair = registry_.find(id);
  if (pair == registry_.end()) {
    return nullptr;
  }
  return &pair->second;
}

const std::vector<NtpPromoIdentifier>&
NtpPromoRegistry::GetNtpPromoIdentifiers() const {
  return identifiers_;
}

void NtpPromoRegistry::AddPromo(NtpPromoSpecification specification) {
  auto [it, inserted] =
      registry_.emplace(specification.id(), std::move(specification));
  CHECK(inserted);
  identifiers_.push_back(it->first);
}

bool NtpPromoRegistry::AreAnyPromosRegistered() const {
  return registry_.size() > 0;
}

void NtpPromoRegistry::ClearPromoForTesting(const NtpPromoIdentifier& id) {
  registry_.erase(id);
  std::erase_if(identifiers_, [id](const NtpPromoIdentifier& identifier) {
    return identifier == id;
  });
}

void NtpPromoRegistry::ClearPromosForTesting() {
  registry_.clear();
  identifiers_.clear();
}

}  // namespace user_education
