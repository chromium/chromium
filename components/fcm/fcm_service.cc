// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fcm/fcm_service.h"

#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "components/fcm/features.h"

namespace fcm {

FcmService::FcmService(std::unique_ptr<FcmDriver> driver)
    : driver_(std::move(driver)) {
  CHECK(base::FeatureList::IsEnabled(kUseFcmService));
  CHECK(driver_);
}

FcmService::~FcmService() = default;

}  // namespace fcm
