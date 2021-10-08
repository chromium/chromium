// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_FEATURES_CHROMEOS_FEATURES_H_
#define CHROMEOS_FEATURES_CHROMEOS_FEATURES_H_

#include "base/feature_list.h"

// This file is only for the feature flags that are shared between ash-chrome
// and lacros-chrome which is not common. For ash features, please add them
// in //ash/constants/ash_features.h.
namespace chromeos {

extern const base::Feature kLacrosTtsSupport;

}  // namespace chromeos

#endif  // CHROMEOS_FEATURES_CHROMEOS_FEATURES_H_
