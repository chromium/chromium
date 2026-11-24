// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_triggered_feature_service.h"

OmniboxTriggeredFeatureService::OmniboxTriggeredFeatureService() = default;
OmniboxTriggeredFeatureService::~OmniboxTriggeredFeatureService() = default;

void OmniboxTriggeredFeatureService::RecordToLogs(
    Features* features_triggered,
    Features* features_triggered_in_session) const {
  *features_triggered = features_triggered_;
  *features_triggered_in_session = features_triggered_in_session_;
}

void OmniboxTriggeredFeatureService::FeatureTriggered(Feature feature) {
  features_triggered_.Put(feature);
  features_triggered_in_session_.Put(feature);
}

bool OmniboxTriggeredFeatureService::GetFeatureTriggeredInSession(
    Feature feature) const {
  return features_triggered_in_session_.Has(feature);
}

void OmniboxTriggeredFeatureService::ResetInput() {
  features_triggered_.Clear();
}

void OmniboxTriggeredFeatureService::ResetSession() {
  features_triggered_.Clear();
  features_triggered_in_session_.Clear();
}
