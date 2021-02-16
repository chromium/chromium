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
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {

// Contains trigger context information for the current script execution.
class TriggerContext {
 public:
  // Creates an empty trigger context.
  TriggerContext();

  // Creates a trigger context with the given values, and default values for
  // all unspecified arguments.
  //
  // NOTE: only use this if you don't intend to send the context to the backend
  // and don't care about the default values. In all other cases, use the full
  // overload below!
  TriggerContext(const std::map<std::string, std::string>& parameters,
                 const std::string& experiment_ids);

  // Creates a trigger context with the given values.
  TriggerContext(const std::map<std::string, std::string>& parameters,
                 const std::string& experiment_ids,
                 bool is_cct,
                 bool onboarding_shown,
                 bool is_direct_action,
                 const std::string& caller_account_hash);

  // Creates a trigger context that contains the merged contents of all input
  // instances at the time of calling (does not reference |contexts| after
  // creation).
  TriggerContext(std::vector<const TriggerContext*> contexts);
  virtual ~TriggerContext();
  TriggerContext(const TriggerContext&) = delete;
  TriggerContext& operator=(const TriggerContext&) = delete;

  // Returns a const reference to all parameters.
  virtual const std::map<std::string, std::string>& GetParameters() const;

  // Returns the value of a specific parameter, if present.
  virtual base::Optional<std::string> GetParameter(
      const std::string& name) const;

  // Getters for specific parameters.
  base::Optional<std::string> GetOverlayColors() const;
  base::Optional<std::string> GetPasswordChangeUsername() const;
  base::Optional<std::string> GetBase64TriggerScriptsResponseProto() const;

  // Returns a comma-separated set of experiment ids.
  virtual std::string GetExperimentIds() const;

  // Returns whether an experiment is contained in |experiment_ids|.
  virtual bool HasExperimentId(const std::string& experiment_id) const;

  // Returns true if we're in a Chrome Custom Tab created for Autofill
  // Assistant, originally created through AutofillAssistantFacade.start(), in
  // Java.
  virtual bool GetCCT() const;

  // Returns true if the onboarding was shown at the beginning when this
  // autofill assistant flow got triggered.
  virtual bool GetOnboardingShown() const;

  // Returns true if the current action was triggered by a direct action.
  virtual bool GetDirectAction() const;

  virtual std::string GetCallerAccountHash() const;

 private:
  // Script parameters provided by the caller.
  std::map<std::string, std::string> parameters_;

  // Experiment ids to be passed to the backend in requests. They may also be
  // used to change client behavior.
  std::string experiment_ids_;

  bool cct_ = false;
  bool onboarding_shown_ = false;
  bool direct_action_ = false;

  std::string caller_account_hash_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_CONTEXT_H_
