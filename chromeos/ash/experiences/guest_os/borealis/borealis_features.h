// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the public base::FeatureList features for Borealis

#ifndef CHROMEOS_ASH_EXPERIENCES_GUEST_OS_BOREALIS_BOREALIS_FEATURES_H_
#define CHROMEOS_ASH_EXPERIENCES_GUEST_OS_BOREALIS_BOREALIS_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace borealis::features {

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kShowBorealisMotd);

}  // namespace borealis::features

#endif  // CHROMEOS_ASH_EXPERIENCES_GUEST_OS_BOREALIS_BOREALIS_FEATURES_H_
