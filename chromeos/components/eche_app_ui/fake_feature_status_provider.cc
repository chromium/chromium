// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/eche_app_ui/fake_feature_status_provider.h"

namespace chromeos {
namespace eche_app {

FakeFeatureStatusProvider::FakeFeatureStatusProvider() {
  status_ = FeatureStatus::kConnected;
}
FakeFeatureStatusProvider::~FakeFeatureStatusProvider() = default;

void FakeFeatureStatusProvider::SetStatus(FeatureStatus status) {
  if (status == status_)
    return;

  status_ = status;
  NotifyStatusChanged();
}

FeatureStatus FakeFeatureStatusProvider::GetStatus() const {
  return status_;
}

}  // namespace eche_app
}  // namespace chromeos
