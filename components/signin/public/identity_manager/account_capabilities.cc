// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <vector>

#include "components/signin/public/identity_manager/account_capabilities.h"

#include "base/no_destructor.h"
#include "components/signin/internal/identity_manager/account_capabilities_constants.h"
#include "components/signin/public/identity_manager/tribool.h"

AccountCapabilities::AccountCapabilities() = default;
AccountCapabilities::~AccountCapabilities() = default;
AccountCapabilities::AccountCapabilities(const AccountCapabilities& other) =
    default;
AccountCapabilities& AccountCapabilities::operator=(
    const AccountCapabilities& other) = default;

// static
const std::vector<std::string>&
AccountCapabilities::GetSupportedAccountCapabilityNames() {
  static base::NoDestructor<std::vector<std::string>> kCapabilityNames{
      {kCanOfferExtendedChromeSyncPromosCapabilityName}};
  return *kCapabilityNames;
}

bool AccountCapabilities::AreAllCapabilitiesKnown() const {
  return can_offer_extended_chrome_sync_promos() != signin::Tribool::kUnknown;
}

signin::Tribool AccountCapabilities::GetCapabilityByName(
    const std::string& name) const {
  const auto iterator = capabilities_map_.find(name);
  if (iterator == capabilities_map_.end()) {
    return signin::Tribool::kUnknown;
  }
  return iterator->second ? signin::Tribool::kTrue : signin::Tribool::kFalse;
}

signin::Tribool AccountCapabilities::can_offer_extended_chrome_sync_promos()
    const {
  return GetCapabilityByName(kCanOfferExtendedChromeSyncPromosCapabilityName);
}

bool AccountCapabilities::UpdateWith(const AccountCapabilities& other) {
  bool modified = false;

  for (const std::string& name : GetSupportedAccountCapabilityNames()) {
    signin::Tribool other_capability = other.GetCapabilityByName(name);
    signin::Tribool current_capability = GetCapabilityByName(name);
    if (other_capability != signin::Tribool::kUnknown &&
        other_capability != current_capability) {
      capabilities_map_[name] = other_capability == signin::Tribool::kTrue;
      modified = true;
    }
  }

  return modified;
}
