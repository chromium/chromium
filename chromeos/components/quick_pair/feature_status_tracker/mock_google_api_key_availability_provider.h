// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_PAIR_FEATURE_STATUS_TRACKER_MOCK_GOOGLE_API_KEY_AVAILABILITY_PROVIDER_H_
#define CHROMEOS_COMPONENTS_QUICK_PAIR_FEATURE_STATUS_TRACKER_MOCK_GOOGLE_API_KEY_AVAILABILITY_PROVIDER_H_

#include "chromeos/components/quick_pair/feature_status_tracker/google_api_key_availability_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {
namespace quick_pair {

class MockGoogleApiKeyAvailabilityProvider
    : public GoogleApiKeyAvailabilityProvider {
 public:
  MockGoogleApiKeyAvailabilityProvider();
  MockGoogleApiKeyAvailabilityProvider(
      const MockGoogleApiKeyAvailabilityProvider&) = delete;
  MockGoogleApiKeyAvailabilityProvider& operator=(
      const MockGoogleApiKeyAvailabilityProvider&) = delete;
  ~MockGoogleApiKeyAvailabilityProvider() override;

  MOCK_METHOD(bool, is_enabled, (), (override));
};

}  // namespace quick_pair
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_QUICK_PAIR_FEATURE_STATUS_TRACKER_MOCK_GOOGLE_API_KEY_AVAILABILITY_PROVIDER_H_
