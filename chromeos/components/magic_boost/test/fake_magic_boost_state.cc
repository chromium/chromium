// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/magic_boost/test/fake_magic_boost_state.h"

#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"

namespace chromeos {
namespace test {

bool FakeMagicBoostState::IsMagicBoostAvailable() {
  return is_magic_boost_available_;
}

bool FakeMagicBoostState::ShouldIncludeOrcaInOptInSync() {
  return false;
}

bool FakeMagicBoostState::CanShowNoticeBannerForHMR() {
  return false;
}

int32_t FakeMagicBoostState::AsyncIncrementHMRConsentWindowDismissCount() {
  return 0;
}

void FakeMagicBoostState::AsyncWriteConsentStatus(
    chromeos::HMRConsentStatus consent_status) {
  UpdateHMRConsentStatus(consent_status);
}

void FakeMagicBoostState::AsyncWriteHMREnabled(bool enabled) {
  UpdateHMREnabled(enabled);
}

void FakeMagicBoostState::SetMagicBoostAvailability(bool available) {
  is_magic_boost_available_ = available;
}

void FakeMagicBoostState::SetMagicBoostEnabled(bool enabled) {
  UpdateMagicBoostEnabled(enabled);
}

}  // namespace test
}  // namespace chromeos
