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

// Abstract base class for trigger context, providing data provided by callers.
class TriggerContext {
 public:
  // Returns an empty trigger context
  static std::unique_ptr<TriggerContext> CreateEmpty();

  // Creates a trigger context with the given values.
  //
  // Takes ownership (through std:move) of the content |params|.
  static std::unique_ptr<TriggerContext> Create(
      std::map<std::string, std::string> params,
      const std::string& exp);

  // Creates a trigger context that references one or more other contexts.
  //
  // The pointers must remain valid for the lifetime of the returned instance.
  static std::unique_ptr<TriggerContext> Merge(
      std::vector<const TriggerContext*> contexts);

  TriggerContext();
  virtual ~TriggerContext();

  // Adds all parameters to the given proto field.
  virtual void AddParameters(
      google::protobuf::RepeatedPtrField<ScriptParameterProto>* dest) const = 0;

  // Returns the value of a specific parameter, if present.
  virtual base::Optional<std::string> GetParameter(
      const std::string& name) const = 0;

  // Returns a comma-separated set of experiment ids.
  virtual std::string experiment_ids() const = 0;

  // Returns true if we're in a Chrome Custom Tab created for Autofill
  // Assistant, originally created through AutofillAssistantFacade.start(), in
  // Java.
  virtual bool is_cct() const = 0;

  // Returns true if the onboarding was shown at the beginning when this
  // autofill assistant flow got triggered.
  virtual bool is_onboarding_shown() const = 0;

  // Returns true if the current action was triggered by a direct action.
  virtual bool is_direct_action() const = 0;
};

// Straightforward implementation of TriggerContext.
class TriggerContextImpl : public TriggerContext {
 public:
  // An empty context
  TriggerContextImpl();

  // Takes ownership (through std:move) of the content |params| and |exp|.
  TriggerContextImpl(std::map<std::string, std::string> params,
                     const std::string& exp);

  ~TriggerContextImpl() override;

  // Implements TriggerContext:
  void AddParameters(google::protobuf::RepeatedPtrField<ScriptParameterProto>*
                         dest) const override;
  base::Optional<std::string> GetParameter(
      const std::string& name) const override;

  void SetCCT(bool value) { cct_ = value; }
  void SetOnboardingShown(bool value) { onboarding_shown_ = value; }
  void SetDirectAction(bool value) { direct_action_ = value; }

  std::string experiment_ids() const override;
  bool is_cct() const override;
  bool is_onboarding_shown() const override;
  bool is_direct_action() const override;

 private:
  // Script parameters provided by the caller.
  std::map<std::string, std::string> parameters_;

  // Experiment ids to be passed to the backend in requests. They may also be
  // used to change client behavior.
  std::string experiment_ids_;

  bool cct_ = false;

  bool direct_action_ = false;

  bool onboarding_shown_ = false;
};

// Merges several TriggerContexts together.
class MergedTriggerContext : public TriggerContext {
 public:
  // The pointers in |contexts| must remain valid for the lifetime of the
  // instances. The vector can be empty.
  MergedTriggerContext(std::vector<const TriggerContext*> contexts);
  ~MergedTriggerContext() override;

  // Implements TriggerContext:
  void AddParameters(google::protobuf::RepeatedPtrField<ScriptParameterProto>*
                         dest) const override;
  base::Optional<std::string> GetParameter(
      const std::string& name) const override;
  std::string experiment_ids() const override;
  bool is_cct() const override;
  bool is_onboarding_shown() const override;
  bool is_direct_action() const override;

 private:
  std::vector<const TriggerContext*> contexts_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_CONTEXT_H_
