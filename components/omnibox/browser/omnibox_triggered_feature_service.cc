// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "omnibox_triggered_feature_service.h"

#include "base/metrics/histogram_functions.h"

OmniboxTriggeredFeatureService::OmniboxTriggeredFeatureService() = default;
OmniboxTriggeredFeatureService::~OmniboxTriggeredFeatureService() = default;

void OmniboxTriggeredFeatureService::RecordToLogs(
    Features* features_triggered,
    Features* features_triggered_in_session) const {
  *features_triggered = features_triggered_;
  *features_triggered_in_session = features_triggered_in_session_;

  bool any_rich_autocompletion_type = false;
  for (const auto& rich_autocompletion_type :
       rich_autocompletion_types_in_session_) {
    base::UmaHistogramEnumeration("Omnibox.RichAutocompletion.Triggered",
                                  rich_autocompletion_type);
    if (rich_autocompletion_type !=
        AutocompleteMatch::RichAutocompletionType::kNone)
      any_rich_autocompletion_type = true;
  }
  base::UmaHistogramBoolean("Omnibox.RichAutocompletion.Triggered.Any",
                            any_rich_autocompletion_type);
}

void OmniboxTriggeredFeatureService::FeatureTriggered(Feature feature) {
  features_triggered_.insert(feature);
  features_triggered_in_session_.insert(feature);
}

void OmniboxTriggeredFeatureService::RichAutocompletionTypeTriggered(
    AutocompleteMatch::RichAutocompletionType rich_autocompletion_type) {
  rich_autocompletion_types_in_session_.insert(rich_autocompletion_type);
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
  rich_autocompletion_types_in_session_.clear();
}
