// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/tribool.h"

bool AccountCapabilities::AreAllCapabilitiesKnown() const {
  return can_offer_extended_chrome_sync_promos_ != signin::Tribool::kUnknown;
}

bool AccountCapabilities::UpdateWith(const AccountCapabilities& other) {
  bool modified = false;

  if (other.can_offer_extended_chrome_sync_promos_ !=
          signin::Tribool::kUnknown &&
      other.can_offer_extended_chrome_sync_promos_ !=
          can_offer_extended_chrome_sync_promos_) {
    can_offer_extended_chrome_sync_promos_ =
        other.can_offer_extended_chrome_sync_promos_;
    modified = true;
  }

  return modified;
}
