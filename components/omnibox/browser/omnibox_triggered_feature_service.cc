// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "omnibox_triggered_feature_service.h"

OmniboxTriggeredFeatureService::OmniboxTriggeredFeatureService() = default;
OmniboxTriggeredFeatureService::~OmniboxTriggeredFeatureService() = default;

void OmniboxTriggeredFeatureService::RecordToLogs(
    Features* features_triggered,
    Features* features_triggered_in_session) const {
  *features_triggered = features_triggered_;
  *features_triggered_in_session = features_triggered_in_session_;
}

void OmniboxTriggeredFeatureService::FeatureTriggered(Feature feature) {
  features_triggered_.insert(feature);
  features_triggered_in_session_.insert(feature);
}

bool OmniboxTriggeredFeatureService::GetFeatureTriggeredInSession(
    Feature feature) const {
  return features_triggered_in_session_.count(feature);
}

void OmniboxTriggeredFeatureService::ResetInput() {
  features_triggered_.clear();
}

void OmniboxTriggeredFeatureService::ResetSession() {
  features_triggered_.clear();
  features_triggered_in_session_.clear();
}
