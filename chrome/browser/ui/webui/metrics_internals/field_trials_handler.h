// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_METRICS_INTERNALS_FIELD_TRIALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_METRICS_INTERNALS_FIELD_TRIALS_HANDLER_H_

#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace variations {
struct StudyGroupNames;
}
class Profile;

// UI Handler for the Field Trials tab of chrome://metrics-internals.
class FieldTrialsHandler : public content::WebUIMessageHandler {
 public:
  explicit FieldTrialsHandler(Profile* profile);
  ~FieldTrialsHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  struct ExperimentOverride {
    std::string trial_hash;
    std::string group_hash;
  };

  // Returns the state of all field trials. Returns a `FieldTrialState` from
  // components/metrics/debug/browser_proxy.ts.
  base::Value::Dict GetFieldTrialStateValue();

  // Handlers for js calls.

  // fetchTrialState() grabs the state of studies and calls populateState() with
  // the result.
  void HandleFetchState(const base::Value::List& args);

  // setTrialEnrollState(callback, trial, group, enabled) overrides the enroll
  // state of a field trial which will be realized after a restart.
  void HandleSetEnrollState(const base::Value::List& args);

  // restart() triggers a restart of Chrome.
  void HandleRestart(const base::Value::List& args);

  // lookupTrialOrGroupName(name) is called when the user types in a a study or
  // experiment name. If the name matches a known study or experiment, this
  // provides the page a mapping from hash to name for presentation.
  void HandleLookupTrialOrGroupName(const base::Value::List& args);

  // One-time initialization for this class.
  void InitializeFieldTrials();

  // Turns on or off an experiment override, which will be realized after a
  // restart.
  bool SetOverride(const ExperimentOverride& override, bool enabled);

  raw_ptr<Profile> profile_;
  bool show_names_ = false;
  bool restart_required_ = false;
  bool initialized_field_trials_ = false;
  std::vector<variations::StudyGroupNames> studies_;
  base::flat_map<std::string, std::string> overrides_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_METRICS_INTERNALS_FIELD_TRIALS_HANDLER_H_
