// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_INPUT_STATE_MODEL_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_INPUT_STATE_MODEL_H_

#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ref.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "third_party/omnibox_proto/aim_input_types.pb.h"
#include "third_party/omnibox_proto/aim_models.pb.h"
#include "third_party/omnibox_proto/aim_tools.pb.h"
#include "third_party/omnibox_proto/searchbox_config.pb.h"

namespace contextual_search {

using omnibox::InputType;
using omnibox::ModelMode;
using omnibox::SearchboxConfig;
using omnibox::ToolMode;

// Represents a valid searchbox inputs state.
// LINT.IfChange(InputState)
struct InputState {
  InputState();
  InputState(const InputState&);
  ~InputState();
  // The set of allowed tools, models, and input types.
  std::vector<ToolMode> allowed_tools;
  std::vector<ModelMode> allowed_models;
  std::vector<InputType> allowed_input_types;
  // The currently active tool and model.
  ToolMode active_tool;
  ModelMode active_model;
  // The set of currently disabled tools, models, and input types.
  std::vector<ToolMode> disabled_tools;
  std::vector<ModelMode> disabled_models;
  std::vector<InputType> disabled_input_types;
};
// LINT.ThenChange(//components/omnibox/composebox/composebox_query.mojom:InputState)

// Manages the state of composebox inputs including tools, models, and
// multimodal inputs.
class InputStateModel {
 public:
  using Subscriber = base::RepeatingCallback<void(const InputState&)>;

  // Constructor takes in a `ContextualSearchSessionHandle` to get uploaded file
  // info.
  explicit InputStateModel(
      contextual_search::ContextualSearchSessionHandle& session_handle,
      const SearchboxConfig& config);
  virtual ~InputStateModel();

  // Add a subscriber to this model.
  base::CallbackListSubscription subscribe(Subscriber callback);

  // Initializes the model and notifies subscribers of the initial state.
  void Initialize();

  // Set a new tool.
  void setActiveTool(ToolMode tool);

  // Set a new model.
  void setActiveModel(ModelMode model);

  // Gets additional query params for the current state.
  std::map<std::string, std::string> GetAdditionalQueryParams();

  // Methods for testing.
  void set_state_for_testing(const InputState& state) { state_ = state; }
  const InputState& get_state_for_testing() { return state_; }

 private:
  // Notify all subscribers of the current `state_`.
  void notifySubscribers();

  // Update the current value of `state_` based on new tool or model.
  void updateSelectedState(ToolMode tool, ModelMode model);

  // Update the currently disabled tools, models, and inputs.
  void updateDisabledState();

  //  Helper method to update `disabled_tools` based on `rule_set_`.
  void UpdateDisabledTools();

  // Helper method to update `disabled_models` based on `rule_set_`.
  void UpdateDisabledModels();

  // Helper method to update `disabled_input_types` based on `rule_set_`.
  void UpdateDisabledInputTypes();

  // Gets the input type limits based on the current state.
  std::map<omnibox::InputType, int> GetInputTypeLimits();

  InputState state_;
  omnibox::RuleSet rule_set_;
  base::raw_ref<contextual_search::ContextualSearchSessionHandle>
      session_handle_;
  base::RepeatingCallbackList<void(const InputState&)> subscribers_;
};

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_INPUT_STATE_MODEL_H_
