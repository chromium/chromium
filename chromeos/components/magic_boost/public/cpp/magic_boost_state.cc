// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"

#include <cstdint>

#include "base/check.h"
#include "base/logging.h"

namespace {
chromeos::MagicBoostState* g_magic_boost_state = nullptr;
}

namespace chromeos {

// static
MagicBoostState* MagicBoostState::Get() {
  return g_magic_boost_state;
}

MagicBoostState::MagicBoostState() {
  CHECK(!g_magic_boost_state);
  g_magic_boost_state = this;
}

MagicBoostState::~MagicBoostState() {
  NotifyOnIsDeleting();

  CHECK_EQ(g_magic_boost_state, this);
  g_magic_boost_state = nullptr;
}

void MagicBoostState::AddObserver(MagicBoostState::Observer* observer) {
  observers_.AddObserver(observer);
}

void MagicBoostState::RemoveObserver(MagicBoostState::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool MagicBoostState::ShouldShowHmrCard() {
  // Should not show if consent_status is `kDeclined` (users explicitly decline
  // in the opt-in flow), or `kUnset` (both Quick Answers and Mahi is not
  // consented to show yet).
  if (hmr_consent_status_ == HMRConsentStatus::kDeclined ||
      hmr_consent_status_ == HMRConsentStatus::kUnset) {
    return false;
  }

  if (hmr_consent_status_.has_value()) {
    CHECK(hmr_consent_status_ == HMRConsentStatus::kApproved ||
          hmr_consent_status_ == HMRConsentStatus::kPendingDisclaimer);
  }

  return true;
}

void MagicBoostState::UpdateMagicBoostEnabled(bool enabled) {
  magic_boost_enabled_ = enabled;

  for (auto& observer : observers_) {
    observer.OnMagicBoostEnabledUpdated(magic_boost_enabled_.value());
  }
}

void MagicBoostState::UpdateHMREnabled(bool enabled) {
  hmr_enabled_ = enabled;

  for (auto& observer : observers_) {
    observer.OnHMREnabledUpdated(hmr_enabled_.value());
  }
}

void MagicBoostState::UpdateHMRConsentStatus(HMRConsentStatus consent_status) {
  hmr_consent_status_ = consent_status;

  for (auto& observer : observers_) {
    observer.OnHMRConsentStatusUpdated(hmr_consent_status_.value());
  }
}

void MagicBoostState::UpdateHMRConsentWindowDismissCount(
    int32_t dismiss_count) {
  hmr_consent_window_dismiss_count_ = dismiss_count;
}

void MagicBoostState::NotifyOnIsDeleting() {
  for (auto& observer : observers_) {
    observer.OnIsDeleting();
  }
}

}  // namespace chromeos
