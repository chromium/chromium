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
#include "components/omnibox/common/input_state.h"
#include "third_party/omnibox_proto/input_type.pb.h"
#include "third_party/omnibox_proto/model_mode.pb.h"
#include "third_party/omnibox_proto/searchbox_config.pb.h"
#include "third_party/omnibox_proto/tool_mode.pb.h"

class PrefService;
namespace contextual_search {

using omnibox::InputState;
using omnibox::InputType;
using omnibox::ModelMode;
using omnibox::SearchboxConfig;
using omnibox::ToolMode;

// Manages the state of composebox inputs including tools, models, and
// multimodal inputs.
class InputStateModel {
 public:
  using Subscriber = base::RepeatingCallback<void(const InputState&)>;

  // Constructor takes in a `ContextualSearchSessionHandle` to get uploaded file
  // info.
  explicit InputStateModel(
      contextual_search::ContextualSearchSessionHandle& session_handle,
      const SearchboxConfig& config,
      bool is_off_the_record);
  InputStateModel(
      const InputStateModel& other,
      contextual_search::ContextualSearchSessionHandle& new_session_handle);
  virtual ~InputStateModel();

  // Add a subscriber to this model.
  base::CallbackListSubscription subscribe(Subscriber callback);

  // Initializes the model and notifies subscribers of the initial state.
  void Initialize();

  // Set a new tool.
  void setActiveTool(ToolMode tool);

  // Set a new model.
  void setActiveModel(ModelMode model);

  // Called when an input of type `InputType` is added or deleted.
  void OnContextChanged();

  // Gets additional query params for the current state.
  std::map<std::string, std::string> GetAdditionalQueryParams();

  // Methods for testing.
  void set_state_for_testing(const InputState& state) { state_ = state; }
  const InputState& get_state_for_testing() { return state_; }

  // Gets the `PrefService`.
  void SetPrefService(const PrefService* pref_service);

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

  // Helper to check if search content sharing is enabled based on the
  // user preference from enterprise policy.
  bool IsSearchContentSharingEnabled() const;

  InputState state_;
  omnibox::RuleSet rule_set_;
  base::WeakPtr<contextual_search::ContextualSearchSessionHandle>
      session_handle_;
  base::RepeatingCallbackList<void(const InputState&)> subscribers_;

  raw_ptr<const PrefService> pref_service_ = nullptr;
  const bool is_off_the_record_;
};

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_INPUT_STATE_MODEL_H_
