// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/input_state_model.h"

#include <map>
#include <set>
#include <vector>

#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/lens/contextual_input.h"
#include "third_party/omnibox_proto/aim_input_types.pb.h"
#include "third_party/omnibox_proto/searchbox_config_constraints.pb.h"

namespace contextual_search {

using omnibox::SearchboxConfig;

InputState::InputState() = default;
InputState::~InputState() = default;

InputStateModel::InputStateModel(
    contextual_search::ContextualSearchSessionHandle& session_handle,
    const SearchboxConfig& config)
    : session_handle_(session_handle) {
  if (config.has_rule_set()) {
    rule_set_ = config.rule_set();

    // Initialize allowed tools, models, inputs in `state_`.
    for (const auto& tool : rule_set_.allowed_tools()) {
      state_.allowed_tools.push_back(static_cast<omnibox::ToolMode>(tool));
    }
    for (const auto& model : rule_set_.allowed_models()) {
      state_.allowed_models.push_back(static_cast<omnibox::ModelMode>(model));
    }
    for (const auto& input_type : rule_set_.allowed_input_types()) {
      state_.allowed_input_types.push_back(
          static_cast<omnibox::InputType>(input_type));
    }
  }

  state_.active_tool = config.has_initial_tool_mode()
                           ? config.initial_tool_mode()
                           : omnibox::ToolMode::TOOL_MODE_UNSPECIFIED;
  state_.active_model = config.has_initial_model_mode()
                            ? config.initial_model_mode()
                            : omnibox::ModelMode::MODEL_MODE_UNSPECIFIED;

  updateDisabledState();
}

InputStateModel::~InputStateModel() = default;

base::CallbackListSubscription InputStateModel::subscribe(Subscriber callback) {
  return subscribers_.Add(std::move(callback));
}

void InputStateModel::notifySubscribers() {
  subscribers_.Notify(state_);
}

namespace {

// Checks if a set of items are all present in an allowed list.
template <typename T, typename U>
bool AreItemsAllowed(const T& items, const U& allowed_items) {
  std::set<typename U::value_type> allowed_set(allowed_items.begin(),
                                               allowed_items.end());
  for (const auto& item : items) {
    if (allowed_set.find(item) == allowed_set.end()) {
      return false;
    }
  }
  return true;
}

// Returns if an item is allowed in a list of items.
template <typename T, typename U>
bool IsItemAllowed(const T& item, const U& allowed_items) {
  std::set<typename U::value_type> allowed_set(allowed_items.begin(),
                                               allowed_items.end());
  return allowed_set.count(item);
}

// Returns the rule for a given `model`.
const omnibox::ModelRule* GetModelRule(const omnibox::RuleSet& rule_set,
                                       ModelMode model) {
  for (const auto& rule : rule_set.model_rules()) {
    if (rule.model() == model) {
      return &rule;
    }
  }
  return nullptr;
}

// Returns a rule for a given `tool`.
const omnibox::ToolRule* GetToolRule(const omnibox::RuleSet& rule_set,
                                     ToolMode tool) {
  for (const auto& rule : rule_set.tool_rules()) {
    if (rule.tool() == tool) {
      return &rule;
    }
  }
  return nullptr;
}

// Gets the current input types from the session handle.
std::vector<omnibox::InputType> GetCurrentInputTypes(
    const contextual_search::ContextualSearchSessionHandle& session_handle) {
  std::vector<omnibox::InputType> input_types;
  for (const auto& file_info : session_handle.GetUploadedContextFileInfos()) {
    switch (file_info.mime_type) {
      case lens::MimeType::kImage:
        input_types.push_back(omnibox::InputType::INPUT_TYPE_LENS_IMAGE);
        break;
      case lens::MimeType::kPdf:
        input_types.push_back(omnibox::InputType::INPUT_TYPE_LENS_FILE);
        break;
      default:
        break;
    }
  }
  return input_types;
}

}  // namespace

void InputStateModel::setActiveTool(ToolMode tool) {
  updateSelectedState(tool, state_.active_model);
  notifySubscribers();
}

void InputStateModel::setActiveModel(ModelMode model) {
  updateSelectedState(state_.active_tool, model);
  notifySubscribers();
}

void InputStateModel::updateSelectedState(ToolMode tool, ModelMode model) {
  // Clear the inputs if the model has changed.
  if (model != state_.active_model) {
    session_handle_.get().ClearFiles();
  }

  state_.active_model = model;
  state_.active_tool = tool;

  // Update the disabled state based on the active model, tool, and current
  // input types.
  updateDisabledState();
}

void InputStateModel::UpdateDisabledTools() {
  // Disable a tool if:
  // - Incompatible with the active model.
  // - Incompatible with the current inputs.
  state_.disabled_tools.clear();
  const omnibox::ModelRule* active_model_rule =
      GetModelRule(rule_set_, state_.active_model);
  for (const auto& tool : state_.allowed_tools) {
    if (tool == state_.active_tool) {
      continue;
    }

    bool incompatible_with_model =
        state_.active_model != omnibox::ModelMode::MODEL_MODE_UNSPECIFIED &&
        (!active_model_rule ||
         !IsItemAllowed(tool, active_model_rule->allowed_tools()));

    const omnibox::ToolRule* tool_rule = GetToolRule(rule_set_, tool);
    bool incompatible_with_inputs =
        !tool_rule ||
        !AreItemsAllowed(GetCurrentInputTypes(session_handle_.get()),
                         tool_rule->allowed_input_types());

    if (incompatible_with_model || incompatible_with_inputs) {
      state_.disabled_tools.push_back(tool);
    }
  }
}

void InputStateModel::UpdateDisabledModels() {
  // Disable a model if:
  // - Incompatible with the active tool.
  // - Incompatible with the current inputs.
  state_.disabled_models.clear();

  for (const auto& model : state_.allowed_models) {
    if (model == state_.active_model) {
      continue;
    }

    const omnibox::ModelRule* model_rule = GetModelRule(rule_set_, model);

    bool incompatible_with_tool =
        state_.active_tool != omnibox::ToolMode::TOOL_MODE_UNSPECIFIED &&
        (!model_rule ||
         !IsItemAllowed(state_.active_tool, model_rule->allowed_tools()));

    bool incompatible_with_model =
        model != omnibox::ModelMode::MODEL_MODE_UNSPECIFIED;

    bool incompatible_with_inputs =
        (!model_rule ||
         !AreItemsAllowed(GetCurrentInputTypes(session_handle_.get()),
                          model_rule->allowed_input_types()));

    if (incompatible_with_model || incompatible_with_tool ||
        incompatible_with_inputs) {
      state_.disabled_models.push_back(model);
    }
  }
}

void InputStateModel::UpdateDisabledInputTypes() {
  // Disable an input type if:
  // - Input type limit is reached.
  // - Total input limit is reached.
  // - Incompatible with the active model.
  // - Incompatible with the active tool.
  state_.disabled_input_types.clear();

  // TODO(crbug.com/476196141): Set disabled inputs based on input limits.
  const omnibox::ModelRule* active_model_rule =
      GetModelRule(rule_set_, state_.active_model);
  const omnibox::ToolRule* active_tool_rule =
      GetToolRule(rule_set_, state_.active_tool);

  for (const auto& input_type : state_.allowed_input_types) {
    bool incompatible_with_model =
        state_.active_model != omnibox::ModelMode::MODEL_MODE_UNSPECIFIED &&
        active_model_rule &&
        !IsItemAllowed(input_type, active_model_rule->allowed_input_types());

    bool incompatible_with_tool =
        state_.active_tool != omnibox::ToolMode::TOOL_MODE_UNSPECIFIED &&
        active_tool_rule &&
        !IsItemAllowed(input_type, active_tool_rule->allowed_input_types());

    if (incompatible_with_model || incompatible_with_tool) {
      state_.disabled_input_types.push_back(input_type);
    }
  }
}

void InputStateModel::updateDisabledState() {
  UpdateDisabledTools();
  UpdateDisabledModels();
  UpdateDisabledInputTypes();
}

}  // namespace contextual_search
