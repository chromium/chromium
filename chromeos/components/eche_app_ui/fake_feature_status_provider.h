// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_ECHE_APP_UI_FAKE_FEATURE_STATUS_PROVIDER_H_
#define CHROMEOS_COMPONENTS_ECHE_APP_UI_FAKE_FEATURE_STATUS_PROVIDER_H_

#include "chromeos/components/eche_app_ui/feature_status_provider.h"

namespace chromeos {
namespace eche_app {

class FakeFeatureStatusProvider : public FeatureStatusProvider {
 public:
  FakeFeatureStatusProvider();
  ~FakeFeatureStatusProvider() override;

  void SetStatus(FeatureStatus status);

  // FeatureStatusProvider:
  FeatureStatus GetStatus() const override;

 private:
  FeatureStatus status_;
};

}  // namespace eche_app
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_ECHE_APP_UI_FAKE_FEATURE_STATUS_PROVIDER_H_
