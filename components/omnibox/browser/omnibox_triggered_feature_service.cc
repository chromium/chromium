// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "omnibox_triggered_feature_service.h"

OmniboxTriggeredFeatureService::OmniboxTriggeredFeatureService() = default;
OmniboxTriggeredFeatureService::~OmniboxTriggeredFeatureService() = default;

void OmniboxTriggeredFeatureService::RecordToLogs(
    Features* feature_triggered_in_session) const {
  *feature_triggered_in_session = features_;
}

void OmniboxTriggeredFeatureService::TriggerFeature(Feature feature) {
  features_.insert(feature);
}

void OmniboxTriggeredFeatureService::ResetSession() {
  features_.clear();
}
