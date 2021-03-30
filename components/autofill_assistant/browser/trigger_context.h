// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_CONTEXT_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_CONTEXT_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "components/autofill_assistant/browser/script_parameters.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {

// Contains trigger context information for the current script execution.
class TriggerContext {
 public:
  // Helper struct to facilitate instantiating this class.
  struct Options {
    Options(const std::string& experiment_ids,
            bool is_cct,
            bool onboarding_shown,
            bool is_direct_action,
            const std::string& initial_url);
    Options();
    ~Options();
    std::string experiment_ids;
    bool is_cct = false;
    bool onboarding_shown = false;
    bool is_direct_action = false;
    std::string initial_url;
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

  // Creates a trigger context with the given values.
  // TODO(arbesser): remove this overload.
  TriggerContext(std::unique_ptr<ScriptParameters> script_parameters,
                 const std::string& experiment_ids,
                 bool is_cct,
                 bool onboarding_shown,
                 bool is_direct_action,
                 const std::string& initial_url);

  // Creates a trigger context that contains the merged contents of all input
  // instances at the time of calling (does not reference |contexts| after
  // creation).
  TriggerContext(std::vector<const TriggerContext*> contexts);
  virtual ~TriggerContext();
  TriggerContext(const TriggerContext&) = delete;
  TriggerContext& operator=(const TriggerContext&) = delete;

  // Returns a const reference to the script parameters.
  virtual const ScriptParameters& GetScriptParameters() const;

  // Returns a comma-separated set of experiment ids.
  virtual std::string GetExperimentIds() const;

  // Returns the initial url. Use with care and prefer the original deeplink
  // where possible, since the initial url might point to a redirect link
  // instead of the target domain.
  virtual std::string GetInitialUrl() const;

  // Returns whether an experiment is contained in |experiment_ids|.
  virtual bool HasExperimentId(const std::string& experiment_id) const;

  // Returns true if we're in a Chrome Custom Tab created for Autofill
  // Assistant, originally created through AutofillAssistantFacade.start(), in
  // Java.
  virtual bool GetCCT() const;

  // Returns true if the onboarding was shown at the beginning when this
  // autofill assistant flow got triggered.
  virtual bool GetOnboardingShown() const;

  // Sets whether an onboarding was shown.
  virtual void SetOnboardingShown(bool onboarding_shown);

  // Returns true if the current action was triggered by a direct action.
  virtual bool GetDirectAction() const;

 private:
  std::unique_ptr<ScriptParameters> script_parameters_;

  // Experiment ids to be passed to the backend in requests. They may also be
  // used to change client behavior.
  std::string experiment_ids_;

  bool cct_ = false;
  bool onboarding_shown_ = false;
  bool direct_action_ = false;

  // The initial url at the time of triggering.
  std::string initial_url_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_CONTEXT_H_
