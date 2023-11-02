// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "omnibox_triggered_feature_service.h"

#include "base/metrics/histogram_functions.h"

OmniboxTriggeredFeatureService::OmniboxTriggeredFeatureService() = default;
OmniboxTriggeredFeatureService::~OmniboxTriggeredFeatureService() = default;

void OmniboxTriggeredFeatureService::RecordToLogs(
    Features* feature_triggered_in_session) const {
  *feature_triggered_in_session = features_;

  bool any_rich_autocompletion_type = false;
  for (const auto& rich_autocompletion_type : rich_autocompletion_types_) {
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
  features_.insert(feature);
}

void OmniboxTriggeredFeatureService::RichAutocompletionTypeTriggered(
    AutocompleteMatch::RichAutocompletionType rich_autocompletion_type) {
  rich_autocompletion_types_.insert(rich_autocompletion_type);
}

void OmniboxTriggeredFeatureService::ResetSession() {
  features_.clear();
  rich_autocompletion_types_.clear();
}
