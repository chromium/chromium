// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_MAGIC_BOOST_TEST_FAKE_MAGIC_BOOST_STATE_H_
#define CHROMEOS_COMPONENTS_MAGIC_BOOST_TEST_FAKE_MAGIC_BOOST_STATE_H_

#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"

namespace chromeos {
namespace test {

class FakeMagicBoostState : public chromeos::MagicBoostState {
 public:
  bool IsMagicBoostAvailable() override;
  bool CanShowNoticeBannerForHMR() override;
  int32_t AsyncIncrementHMRConsentWindowDismissCount() override;
  void AsyncWriteConsentStatus(
      chromeos::HMRConsentStatus consent_status) override;
  void AsyncWriteHMREnabled(bool enabled) override;
  void DisableOrcaFeature() override {}
};

}  // namespace test
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_MAGIC_BOOST_TEST_FAKE_MAGIC_BOOST_STATE_H_
