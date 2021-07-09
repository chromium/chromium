// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_H_

// Stores the information about account capabilities. Capabilities provide
// information about state and features of Gaia accounts.
class AccountCapabilities {
 public:
  // TODO(https://crbug.com/1213071): share this enum with AccountInfo.
  enum class Tribool { kTrue, kFalse, kUnknown };

  // Chrome can offer extended promos for turning on Sync to accounts with this
  // capability.
  Tribool can_offer_extended_chrome_sync_promos() const {
    return can_offer_extended_chrome_sync_promos_;
  }

  void set_can_offer_extended_chrome_sync_promos(bool value) {
    can_offer_extended_chrome_sync_promos_ =
        value ? Tribool::kTrue : Tribool::kFalse;
  }

  // Whether none of the capabilities has `Tribool::kUnknown`.
  bool AreAllCapabilitiesKnown() const;

  // Updates the fields of `this` with known fields of `other`. Returns whether
  // at least one field was updated.
  bool UpdateWith(const AccountCapabilities& other);

 private:
  Tribool can_offer_extended_chrome_sync_promos_ = Tribool::kUnknown;
};

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_H_
