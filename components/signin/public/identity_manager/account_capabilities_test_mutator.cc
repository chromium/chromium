// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"

#include "components/signin/internal/identity_manager/account_capabilities_constants.h"

AccountCapabilitiesTestMutator::AccountCapabilitiesTestMutator(
    AccountCapabilities* capabilities)
    : capabilities_(capabilities) {}

void AccountCapabilitiesTestMutator::set_can_offer_extended_chrome_sync_promos(
    bool value) {
  capabilities_
      ->capabilities_map_[kCanOfferExtendedChromeSyncPromosCapabilityName] =
      value;
}
