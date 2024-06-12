// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/features.h"

#include "build/build_config.h"

BASE_FEATURE(kClickToCall, "ClickToCall", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSharingMatchPulseInterval,
             "SharingMatchPulseInterval",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kSharingPulseDeltaDesktopHours = {
    &kSharingMatchPulseInterval, "SharingPulseDeltaDesktopHours", 24};

const base::FeatureParam<int> kSharingPulseDeltaAndroidHours = {
    &kSharingMatchPulseInterval, "SharingPulseDeltaAndroidHours", 24};
