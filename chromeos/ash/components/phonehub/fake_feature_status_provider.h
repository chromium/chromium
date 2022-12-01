// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_FEATURE_STATUS_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_FEATURE_STATUS_PROVIDER_H_

#include "chromeos/ash/components/phonehub/feature_status_provider.h"

namespace ash {
namespace phonehub {

class FakeFeatureStatusProvider : public FeatureStatusProvider {
 public:
  // Defaults initial status to kEnabledAndConnected.
  FakeFeatureStatusProvider();
  FakeFeatureStatusProvider(FeatureStatus initial_status);
  ~FakeFeatureStatusProvider() override;

  void SetStatus(FeatureStatus status);

  // FeatureStatusProvider:
  FeatureStatus GetStatus() const override;

 private:
  FeatureStatus status_;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_FEATURE_STATUS_PROVIDER_H_
