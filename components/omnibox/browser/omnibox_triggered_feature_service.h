// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_TRIGGERED_FEATURE_SERVICE_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_TRIGGERED_FEATURE_SERVICE_H_

#include <set>

#include "components/omnibox/browser/autocomplete_match.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

// Tracks the features that trigger during an omnibox session and records them
// to the logs. This is used for counterfactual slicing metrics by feature.
class OmniboxTriggeredFeatureService {
 public:
  using Feature = metrics::OmniboxEventProto_Feature;
  using Features = std::set<Feature>;

  OmniboxTriggeredFeatureService();
  ~OmniboxTriggeredFeatureService();

  // Records `features_triggered_[in_session_]` to the params. Records UMA
  // histograms for any non-omnibox-event-protobuf features (i.e.
  // `rich_autocompletion_types_`).
  void RecordToLogs(Features* features_triggered,
                    Features* features_triggered_in_session) const;

  // Invoked to indicate `feature` was triggered.
  void FeatureTriggered(Feature feature);

  // Invoked to indicate `rich_autocompletion_type` was triggered. Multiple
  // types can be triggered in a session. Does not automatically trigger
  // `kRichAutocompletion`.
  void RichAutocompletionTypeTriggered(
      AutocompleteMatch::RichAutocompletionType rich_autocompletion_type);

  // Returns whether `FeatureTriggered()` was called with `feature` since the
  // last `ResetSession()`.
  bool GetFeatureTriggeredInSession(Feature feature) const;

  // Invoked when a new omnibox input starts. Clears `features_triggered_`.
  void ResetInput();

  // Invoked when a new omnibox session starts. Clears `features_triggered_`,
  // `features_triggered_in_session_`, and
  // `rich_autocompletion_types_in_session_`.
  void ResetSession();

 private:
  // The set of features triggered in the current omnibox session via
  // `FeatureTriggered()`.
  std::set<Feature> features_triggered_;
  std::set<Feature> features_triggered_in_session_;

  // The set of rich autocompletion types triggered in the current omnibox
  // session via `RichAutocompletionTypeTriggered()`.
  std::set<AutocompleteMatch::RichAutocompletionType>
      rich_autocompletion_types_in_session_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_TRIGGERED_FEATURE_SERVICE_H_
