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
InputState::InputState(const InputState&) = default;
InputState::~InputState() = default;

namespace {

// Populates `InputTypeRule` for `omnibox::INPUT_TYPE_BROWSER_TAB` if it does
// not exist.
void MaybePopulateBrowserTabInputTypeRule(omnibox::SearchboxConfig* config) {
  if (!config) {
    return;
  }
  omnibox::RuleSet* rule_set = config->mutable_rule_set();

  bool browser_tab_rule_exists = false;
  for (const auto& rule : rule_set->input_type_rules()) {
    if (rule.input_type() == omnibox::INPUT_TYPE_BROWSER_TAB) {
      browser_tab_rule_exists = true;
      break;
    }
  }

  // Populate `InputTypeRule` for `omnibox::INPUT_TYPE_BROWSER_TAB`.
  if (!browser_tab_rule_exists) {
    omnibox::InputTypeRule* new_rule = rule_set->add_input_type_rules();
    new_rule->set_input_type(omnibox::INPUT_TYPE_BROWSER_TAB);
    new_rule->set_max_instance(5);
    new_rule->add_allowed_input_types(omnibox::INPUT_TYPE_LENS_IMAGE);
    new_rule->add_allowed_input_types(omnibox::INPUT_TYPE_LENS_FILE);
    new_rule->add_allowed_input_types(omnibox::INPUT_TYPE_BROWSER_TAB);
  }

  // Add `omnibox::INPUT_TYPE_BROWSER_TAB` to the `allowed_input_types` in
  // `ToolRule` for all tools if the tool allows both images and files.
  for (auto& tool_rule : *rule_set->mutable_tool_rules()) {
    bool has_image = false;
    bool has_file = false;
    for (const auto& input_type : tool_rule.allowed_input_types()) {
      if (input_type == omnibox::INPUT_TYPE_LENS_IMAGE) {
        has_image = true;
      } else if (input_type == omnibox::INPUT_TYPE_LENS_FILE) {
        has_file = true;
      }
    }
    if (has_image && has_file) {
      tool_rule.add_allowed_input_types(omnibox::INPUT_TYPE_BROWSER_TAB);
    }
  }

  // Add `omnibox::INPUT_TYPE_BROWSER_TAB` to the `allowed_input_types` in
  // `ModelRule` for all models if the model allows both images and files.
  for (auto& model_rule : *rule_set->mutable_model_rules()) {
    bool has_image = false;
    bool has_file = false;
    for (const auto& input_type : model_rule.allowed_input_types()) {
      if (input_type == omnibox::INPUT_TYPE_LENS_IMAGE) {
        has_image = true;
      } else if (input_type == omnibox::INPUT_TYPE_LENS_FILE) {
        has_file = true;
      }
    }
    if (has_image && has_file) {
      model_rule.add_allowed_input_types(omnibox::INPUT_TYPE_BROWSER_TAB);
    }
  }
}

}  // namespace

InputStateModel::InputStateModel(
    contextual_search::ContextualSearchSessionHandle& session_handle,
    const SearchboxConfig& config)
    : session_handle_(session_handle) {
  SearchboxConfig mutable_config = config;
  MaybePopulateBrowserTabInputTypeRule(&mutable_config);

  if (mutable_config.has_rule_set()) {
    rule_set_ = mutable_config.rule_set();

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

  // TODO(crbug.com/479254789): Once `INPUT_TYPE_BROWSER_TAB` is available from
  // server, remove this check.
  if (std::find(state_.allowed_input_types.begin(),
                state_.allowed_input_types.end(),
                omnibox::INPUT_TYPE_BROWSER_TAB) ==
      state_.allowed_input_types.end()) {
    state_.allowed_input_types.push_back(omnibox::INPUT_TYPE_BROWSER_TAB);
  }

  state_.active_tool = mutable_config.has_initial_tool_mode()
                           ? mutable_config.initial_tool_mode()
                           : omnibox::ToolMode::TOOL_MODE_UNSPECIFIED;
  state_.active_model = mutable_config.has_initial_model_mode()
                            ? mutable_config.initial_model_mode()
                            : omnibox::ModelMode::MODEL_MODE_UNSPECIFIED;

  updateDisabledState();
}

InputStateModel::~InputStateModel() = default;

void InputStateModel::Initialize() {
  notifySubscribers();
}

base::CallbackListSubscription InputStateModel::subscribe(Subscriber callback) {
  return subscribers_.Add(std::move(callback));
}

void InputStateModel::notifySubscribers() {
  subscribers_.Notify(state_);
}

namespace {

// Returns if an item is allowed in a list of items.
template <typename T, typename U>
bool IsItemAllowed(const T& item, const U& allowed_items) {
  return std::find(allowed_items.begin(), allowed_items.end(), item) !=
         allowed_items.end();
}

// Checks if a set of items are all present in an allowed list.
template <typename T, typename U>
bool AreItemsAllowed(const T& items, const U& allowed_items) {
  return std::all_of(items.begin(), items.end(),
                     [&allowed_items](const auto& item) {
                       return IsItemAllowed(item, allowed_items);
                     });
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
    if (file_info.tab_url) {
      input_types.push_back(omnibox::InputType::INPUT_TYPE_BROWSER_TAB);
      continue;
    }
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
}

void InputStateModel::setActiveModel(ModelMode model) {
  updateSelectedState(state_.active_tool, model);
}

void InputStateModel::updateSelectedState(ToolMode tool, ModelMode model) {
  state_.active_model = model;
  state_.active_tool = tool;

  // Update the disabled state based on the active model, tool, and current
  // input types.
  updateDisabledState();

  // Notify subscribers once `state_` is updated.
  notifySubscribers();
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
  // - Another model is active.
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

    // If a model is already active, all other models are disabled.
    bool another_model_active =
        state_.active_model != omnibox::ModelMode::MODEL_MODE_UNSPECIFIED;

    bool incompatible_with_inputs =
        (!model_rule ||
         !AreItemsAllowed(GetCurrentInputTypes(session_handle_.get()),
                          model_rule->allowed_input_types()));

    if (another_model_active || incompatible_with_tool ||
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

  std::map<omnibox::InputType, int> limits = GetInputTypeLimits();
  std::map<omnibox::InputType, int> current_input_counts;
  for (const auto& input_type : GetCurrentInputTypes(session_handle_.get())) {
    current_input_counts[input_type]++;
  }

  const omnibox::ModelRule* active_model_rule =
      GetModelRule(rule_set_, state_.active_model);
  const omnibox::ToolRule* active_tool_rule =
      GetToolRule(rule_set_, state_.active_tool);

  for (const auto& input_type : state_.allowed_input_types) {
    bool input_limit_reached = false;
    if (limits.count(input_type)) {
      int limit = limits.at(input_type);
      if (limit > 0 && current_input_counts.count(input_type) &&
          current_input_counts.at(input_type) >= limit) {
        input_limit_reached = true;
      }
    }

    bool incompatible_with_model =
        state_.active_model != omnibox::ModelMode::MODEL_MODE_UNSPECIFIED &&
        active_model_rule &&
        !IsItemAllowed(input_type, active_model_rule->allowed_input_types());

    bool incompatible_with_tool =
        state_.active_tool != omnibox::ToolMode::TOOL_MODE_UNSPECIFIED &&
        active_tool_rule &&
        !IsItemAllowed(input_type, active_tool_rule->allowed_input_types());

    if (input_limit_reached || incompatible_with_model ||
        incompatible_with_tool) {
      state_.disabled_input_types.push_back(input_type);
    }
  }
}

void InputStateModel::updateDisabledState() {
  UpdateDisabledTools();
  UpdateDisabledModels();
  UpdateDisabledInputTypes();
}

std::map<omnibox::InputType, int> InputStateModel::GetInputTypeLimits() {
  std::map<omnibox::InputType, int> limits;
  for (const auto& rule : rule_set_.input_type_rules()) {
    if (rule.has_input_type() && rule.has_max_instance()) {
      limits[rule.input_type()] = rule.max_instance();
    }
  }
  return limits;
}

std::map<std::string, std::string> InputStateModel::GetAdditionalQueryParams() {
  std::map<std::string, std::string> additional_params;
  switch (state_.active_tool) {
    case omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH:
      additional_params["dr"] = "1";
      break;
    case omnibox::ToolMode::TOOL_MODE_CANVAS:
      additional_params["rc"] = "1";
      break;
    case omnibox::ToolMode::TOOL_MODE_IMAGE_GEN:
    case omnibox::ToolMode::TOOL_MODE_IMAGE_GEN_UPLOAD:
      additional_params["imgn"] = "1";
      break;
    default:
      break;
  }

  switch (state_.active_model) {
    case omnibox::ModelMode::MODEL_MODE_GEMINI_PRO:
      additional_params["m"] = "1";
      break;
    case omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_AUTOROUTE:
      additional_params["m"] = "2";
      break;
    default:
      break;
  }
  return additional_params;
}

}  // namespace contextual_search
