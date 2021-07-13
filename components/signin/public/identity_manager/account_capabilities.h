// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_H_

#include "components/signin/public/identity_manager/tribool.h"

// Stores the information about account capabilities. Capabilities provide
// information about state and features of Gaia accounts.
class AccountCapabilities {
 public:

  // Chrome can offer extended promos for turning on Sync to accounts with this
  // capability.
  signin::Tribool can_offer_extended_chrome_sync_promos() const {
    return can_offer_extended_chrome_sync_promos_;
  }

  void set_can_offer_extended_chrome_sync_promos(bool value) {
    can_offer_extended_chrome_sync_promos_ =
        value ? signin::Tribool::kTrue : signin::Tribool::kFalse;
  }

  // Whether none of the capabilities has `signin::Tribool::kUnknown`.
  bool AreAllCapabilitiesKnown() const;

  // Updates the fields of `this` with known fields of `other`. Returns whether
  // at least one field was updated.
  bool UpdateWith(const AccountCapabilities& other);

 private:
  signin::Tribool can_offer_extended_chrome_sync_promos_ =
      signin::Tribool::kUnknown;
};

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_H_
