// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_CONTEXT_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_CONTEXT_H_

#include <memory>
#include <string>
#include <vector>

#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {

class ScriptParameters;

// Contains trigger context information for the current script execution.
class TriggerContext {
 public:
  // Helper struct to facilitate instantiating this class.
  struct Options {
    Options();
    Options(const std::string& experiment_ids,
            bool is_cct,
            bool onboarding_shown,
            bool is_direct_action,
            const std::string& initial_url,
            bool is_in_chrome_triggered,
            bool is_externally_triggered,
            bool skip_autofill_assistant_onboarding,
            bool suppress_browsing_features);
    ~Options();

    std::string experiment_ids;
    bool is_cct = false;
    bool onboarding_shown = false;
    bool is_direct_action = false;
    std::string initial_url;
    bool is_in_chrome_triggered = false;
    bool is_externally_triggered = false;
    bool skip_autofill_assistant_onboarding = false;
    bool suppress_browsing_features = true;
  };

  // Creates an empty trigger context.
  TriggerContext();

  // Creates a trigger context with the given values, and default values for
  // all unspecified arguments.
  //
  // NOTE: always specify the full set of options for instances that you intend
  // to send to the backend!
  TriggerContext(std::unique_ptr<ScriptParameters> script_parameters,
                 const Options& options);

  // Creates a trigger context that contains the merged contents of all input
  // instances at the time of calling (does not reference |contexts| after
  // creation).
  explicit TriggerContext(std::vector<const TriggerContext*> contexts);

  ~TriggerContext();
  TriggerContext(const TriggerContext&) = delete;
  TriggerContext& operator=(const TriggerContext&) = delete;

  // Returns a const reference to the script parameters.
  const ScriptParameters& GetScriptParameters() const;

  // Replaces the current script parameters with `script_parameters`.
  void SetScriptParameters(std::unique_ptr<ScriptParameters> script_parameters);

  // Returns a comma-separated set of experiment ids.
  std::string GetExperimentIds() const;

  // Returns the initial url. Use with care and prefer the original deeplink
  // where possible, since the initial url might point to a redirect link
  // instead of the target domain.
  std::string GetInitialUrl() const;

  // Returns whether an experiment is contained in `experiment_ids`.
  bool HasExperimentId(const std::string& experiment_id) const;

  // Returns true if we're in a Chrome Custom Tab created for Autofill
  // Assistant, originally created through `AutofillAssistantFacade.start()` in
  // Java.
  bool GetCCT() const;

  // Returns true if the onboarding was shown at the beginning when this
  // autofill assistant flow got triggered.
  bool GetOnboardingShown() const;

  // Sets whether an onboarding was shown.
  void SetOnboardingShown(bool onboarding_shown);

  // Returns true if the current action was triggered by a direct action.
  bool GetDirectAction() const;

  // Returns whether this trigger context is coming from an external surface,
  // i.e., a button or link on a website, or whether this is from within
  // Chrome.
  bool GetInChromeTriggered() const;

  // Returns whether the triggering source is external, i.e. headless.
  bool GetIsExternallyTriggered() const;

  // Returns whether the triggering source will handle its own onboarding flow
  // and the default onboarding flow should be skipped.
  bool GetSkipAutofillAssistantOnboarding() const;

  // Returns whether browsing features, such as the keyboard, Autofill,
  // translation, etc.  should be suppressed while a flow is running.
  bool GetSuppressBrowsingFeatures() const;

  // Returns the trigger type of the trigger script that was shown and accepted
  // at the beginning of the flow, if any.
  TriggerScriptProto::TriggerUIType GetTriggerUIType() const;

  // Sets the trigger type of the shown trigger script.
  void SetTriggerUIType(TriggerScriptProto::TriggerUIType trigger_ui_type);

 private:
  std::unique_ptr<ScriptParameters> script_parameters_;

  // Experiment ids to be passed to the backend in requests. They may also be
  // used to change client behavior.
  std::string experiment_ids_;

  bool cct_ = false;
  bool onboarding_shown_ = false;
  bool direct_action_ = false;
  bool is_in_chrome_triggered_ = false;
  bool is_externally_triggered_ = false;
  bool skip_autofill_assistant_onboarding_ = false;
  bool suppress_browsing_features_ = true;

  // The initial url at the time of triggering.
  std::string initial_url_;
  TriggerScriptProto::TriggerUIType trigger_ui_type_ =
      TriggerScriptProto::UNSPECIFIED_TRIGGER_UI_TYPE;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_CONTEXT_H_
