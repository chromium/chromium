// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MESSAGES_ANDROID_MESSAGES_FEATURE_H_
#define COMPONENTS_MESSAGES_ANDROID_MESSAGES_FEATURE_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace messages {

// Feature that allows for AccessibilityEvents to be sent in Java-side impl to
// test possible crash solutions.
BASE_DECLARE_FEATURE(kMessagesAccessibilityEventInvestigations);

// A feature param of type int that corresponds to the possible approaches for
// fixing the crash.
const base::FeatureParam<int> kMessagesAccessibilityEventInvestigationsParam{
    &kMessagesAccessibilityEventInvestigations,
    "messages_accessibility_events_investigations_param", 0};

// Feature that exposes a listener to notify whether the current message
// is fully visible.
BASE_DECLARE_FEATURE(kMessagesForAndroidFullyVisibleCallback);

// Feature that enables extra histogram recordings.
BASE_DECLARE_FEATURE(kMessagesAndroidExtraHistograms);

// Feature that enables a close button when mouses hovers over.
BASE_DECLARE_FEATURE(kMessagesCloseButton);

}  // namespace messages

#endif  // COMPONENTS_MESSAGES_ANDROID_MESSAGES_FEATURE_H_
