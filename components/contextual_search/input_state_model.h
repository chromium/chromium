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
#include "components/prefs/pref_change_registrar.h"
#include "third_party/omnibox_proto/input_type.pb.h"
#include "third_party/omnibox_proto/model_mode.pb.h"
#include "third_party/omnibox_proto/searchbox_config.pb.h"
#include "third_party/omnibox_proto/tool_mode.pb.h"

class GURL;
class PrefService;
namespace contextual_search {

using omnibox::InputState;
using omnibox::InputType;
using omnibox::ModelMode;
using omnibox::SearchboxConfig;
using omnibox::ToolMode;

// Enum values are persisted to preferences (as integers). Do not reorder,
// delete, or reuse values.
enum class DriveConsentState {
  kNotReady = 0,
  kRestricted = 1,
  kConsent = 2,
  kNotConsent = 3
};

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
      const GURL& active_url,
      bool is_off_the_record,
      bool browser_identity_matches_aim_identity);
  InputStateModel(
      const InputStateModel& other,
      contextual_search::ContextualSearchSessionHandle& new_session_handle);
  virtual ~InputStateModel();

  base::WeakPtr<InputStateModel> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Returns the current input types from the session handle.
  static std::vector<InputType> GetCurrentInputTypes(
      const contextual_search::ContextualSearchSessionHandle* session_handle);

  // Add a subscriber to this model.
  base::CallbackListSubscription subscribe(Subscriber callback);

  // Initializes the model and notifies subscribers of the initial state.
  void Initialize();

  // Set a new tool.
  void setActiveTool(ToolMode tool);

  // Set a new model.
  void setActiveModel(ModelMode model);
  void UpdateStateFromUrl(const GURL& url);

  // Called when an input of type `InputType` is added or deleted.
  void OnContextChanged();

  // Sets the tools that should be forced to be disabled.
  void SetPermanentlyDisabledTools(const std::vector<ToolMode>& tools);

  // Sets the input types that should be forced to be disabled.
  void SetPermanentlyDisabledInputTypes(
      const std::vector<InputType>& input_types);


  // Gets additional query params for the current state.
  std::map<std::string, std::string> GetAdditionalQueryParams();

  // Returns the current state.
  const InputState& GetInputState() const;

  contextual_search::ContextualSearchSessionHandle* session_handle() const {
    return session_handle_.get();
  }

  // Methods for testing.
  void set_state_for_testing(const InputState& state) { state_ = state; }
  const InputState& get_state_for_testing() { return state_; }
  bool browser_identity_matches_aim_identity_for_testing() const {
    return browser_identity_matches_aim_identity_;
  }

  DriveConsentState drive_consent_state_for_testing() const {
    return drive_consent_state_;
  }

  // Gets the `PrefService`.
  void SetPrefService(PrefService* pref_service);

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

  // Helper to check if the Drive input type is supported.
  bool IsDriveSupported() const;

  // Invoked as a callback by the PrefChangeRegistrar when the observed
  // user preferences change. Re-reads the current preference values, updates
  // the model's internal allowed input state, and notifies all subscribers.
  void OnPrefChanged();

  // Rebuilds allowed input types based on config, sharing, and drive status.
  void RebuildAllowedInputTypes();

  // Returns the rule for a given `model`.
  const omnibox::ModelRule* GetModelRule(ModelMode model) const;

  // Returns a rule for a given `tool`.
  const omnibox::ToolRule* GetToolRule(ToolMode tool) const;

  InputState state_;
  omnibox::RuleSet rule_set_;
  base::WeakPtr<contextual_search::ContextualSearchSessionHandle>
      session_handle_;
  base::RepeatingCallbackList<void(const InputState&)> subscribers_;

  raw_ptr<PrefService> pref_service_ = nullptr;
  PrefChangeRegistrar pref_change_registrar_;
  const bool is_off_the_record_;
  const bool browser_identity_matches_aim_identity_;
  GURL current_url_;

  // Configured input types from the searchbox configuration.
  std::vector<omnibox::InputType> configured_input_types_;

  // Stores tools that are permanently disabled by an external trigger and must
  // persist through state updates. Persists after Initialize() is called.
  std::vector<ToolMode> permanently_disabled_tools_;
  // Stores input_types that are permanently disabled by an external trigger and
  // must persist through state updates. Persists after Initialize() is called.
  std::vector<InputType> permanently_disabled_input_types_;

  DriveConsentState drive_consent_state_ = DriveConsentState::kNotReady;

  base::WeakPtrFactory<InputStateModel> weak_ptr_factory_{this};
};

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_INPUT_STATE_MODEL_H_
