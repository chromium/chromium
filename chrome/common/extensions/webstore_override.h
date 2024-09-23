// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_WEBSTORE_OVERRIDE_H_
#define CHROME_COMMON_EXTENSIONS_WEBSTORE_OVERRIDE_H_

#include "extensions/common/features/feature.h"

namespace extensions::webstore_override {

Feature::FeatureDelegatedAvailabilityCheckMap CreateAvailabilityCheckMap();

}  // namespace extensions::webstore_override

#endif  // CHROME_COMMON_EXTENSIONS_WEBSTORE_OVERRIDE_H_
