// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CHROMEOS_FEATURES_H_
#define CHROMEOS_CHROMEOS_FEATURES_H_

#include "base/feature_list.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

CHROMEOS_EXPORT extern const base::Feature kAndroidMessagesIntegration;
CHROMEOS_EXPORT extern const base::Feature kAndroidMessagesProdEndpoint;
CHROMEOS_EXPORT extern const base::Feature kAutoScreenBrightness;
CHROMEOS_EXPORT extern const base::Feature kChromeVoxArcSupport;
CHROMEOS_EXPORT extern const base::Feature kDriveFs;
CHROMEOS_EXPORT extern const base::Feature kMyFilesVolume;
CHROMEOS_EXPORT extern const base::Feature kEnableUnifiedMultiDeviceSettings;
CHROMEOS_EXPORT extern const base::Feature kEnableUnifiedMultiDeviceSetup;
CHROMEOS_EXPORT extern const base::Feature kImeServiceConnectable;
CHROMEOS_EXPORT extern const base::Feature kInstantTethering;
CHROMEOS_EXPORT extern const base::Feature kMultiDeviceApi;
CHROMEOS_EXPORT extern const base::Feature kUserActivityPrediction;

}  // namespace features

}  // namespace chromeos

#endif  // CHROMEOS_CHROMEOS_FEATURES_H_
