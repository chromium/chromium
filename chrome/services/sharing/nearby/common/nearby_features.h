// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_COMMON_NEARBY_FEATURES_H_
#define CHROME_SERVICES_SHARING_NEARBY_COMMON_NEARBY_FEATURES_H_

#include "base/feature_list.h"

namespace features {

BASE_DECLARE_FEATURE(kEnableNearbyBleV2ExtendedAdvertising);
bool IsNearbyBleV2ExtendedAdvertisingEnabled();

}  // namespace features

#endif  // CHROME_SERVICES_SHARING_NEARBY_COMMON_NEARBY_FEATURES_H_
