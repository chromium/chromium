// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/input_state_model.h"

#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/contextual_search/pref_names.h"
#include "components/lens/contextual_input.h"
#include "components/prefs/pref_service.h"
#include "third_party/omnibox_proto/input_type.pb.h"
#include "third_party/omnibox_proto/rule_set.pb.h"

namespace contextual_search {

using omnibox::SearchboxConfig;

namespace {

// Populates `InputTypeRule` for `omnibox::INPUT_TYPE_BROWSER_TAB` if it does
// not exist.
void MaybePopulateBrowserTabInputTypeRule(omnibox::SearchboxConfig* config) {
  if (!config) {
    return;
  }
  omnibox::RuleSet* rule_set = config->mutable_rule_set();

  // The default max_instance for tabs is 5.
  int max_browser_tab_instances = 5;

  bool browser_tab_rule_exists = false;
  for (const auto& rule : rule_set->input_type_rules()) {
    // Until we get browser tab rules, treat browser tab input as image input
    // for max instance limit purposes.
    if (rule.input_type() == omnibox::INPUT_TYPE_LENS_IMAGE) {
      max_browser_tab_instances = rule.max_instance();
    }
    if (rule.input_type() == omnibox::INPUT_TYPE_BROWSER_TAB) {
      browser_tab_rule_exists = true;
      break;
    }
  }

  // Populate `InputTypeRule` for `omnibox::INPUT_TYPE_BROWSER_TAB`.
  if (!browser_tab_rule_exists) {
    omnibox::InputTypeRule* new_rule = rule_set->add_input_type_rules();
    new_rule->set_input_type(omnibox::INPUT_TYPE_BROWSER_TAB);
    new_rule->set_max_instance(max_browser_tab_instances);
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
    const SearchboxConfig& config,
    bool is_off_the_record)
    : session_handle_(session_handle.AsWeakPtr()),
      is_off_the_record_(is_off_the_record) {
  SearchboxConfig mutable_config = config;
  MaybePopulateBrowserTabInputTypeRule(&mutable_config);

  if (mutable_config.has_rule_set()) {
    rule_set_ = mutable_config.rule_set();

    // Initialize allowed tools, models, inputs in `state_`.
    state_.allowed_tools.reserve(rule_set_.allowed_tools().size());
    for (const auto& tool : rule_set_.allowed_tools()) {
      if (tool == omnibox::ToolMode::TOOL_MODE_IMAGE_GEN_UPLOAD) {
        continue;
      }
      if (is_off_the_record_ &&
          tool == omnibox::ToolMode::TOOL_MODE_IMAGE_GEN) {
        continue;
      }
      state_.allowed_tools.push_back(static_cast<omnibox::ToolMode>(tool));
    }
    state_.allowed_models.reserve(rule_set_.allowed_models().size());
    for (const auto& model : rule_set_.allowed_models()) {
      state_.allowed_models.push_back(static_cast<omnibox::ModelMode>(model));
    }
    state_.allowed_input_types.reserve(rule_set_.allowed_input_types().size());
    for (const auto& input_type : rule_set_.allowed_input_types()) {
      state_.allowed_input_types.push_back(
          static_cast<omnibox::InputType>(input_type));
    }
    state_.tool_configs.reserve(mutable_config.tool_configs_size());
    for (const auto& tool_config : mutable_config.tool_configs()) {
      state_.tool_configs.push_back(tool_config);
    }
    state_.model_configs.reserve(mutable_config.model_configs_size());
    for (const auto& model_config : mutable_config.model_configs()) {
      state_.model_configs.push_back(model_config);
    }
    state_.input_type_configs.reserve(mutable_config.input_type_configs_size());
    for (const auto& input_type_config : mutable_config.input_type_configs()) {
      state_.input_type_configs.push_back(input_type_config);
    }
    if (mutable_config.has_tools_section_config()) {
      state_.tools_section_config = mutable_config.tools_section_config();
    }
    if (mutable_config.has_model_section_config()) {
      state_.model_section_config = mutable_config.model_section_config();
    }
    if (mutable_config.has_hint_text()) {
      state_.hint_text = mutable_config.hint_text();
    }
    if (rule_set_.has_max_total_inputs()) {
      state_.max_total_inputs = rule_set_.max_total_inputs();
    }
    for (const auto& rule : rule_set_.input_type_rules()) {
      if (rule.has_input_type() && rule.has_max_instance()) {
        state_.max_instances[rule.input_type()] = rule.max_instance();
      }
    }
  }

  // TODO(crbug.com/479254789): Once `INPUT_TYPE_BROWSER_TAB` is available from
  // server, remove this check.
  auto contains = [&](omnibox::InputType type) {
    return std::find(state_.allowed_input_types.begin(),
                     state_.allowed_input_types.end(),
                     type) != state_.allowed_input_types.end();
  };

  // Only add browser tab if it does not already exist and both lens and image
  // types are allowed.
  if (!contains(omnibox::INPUT_TYPE_BROWSER_TAB) &&
      contains(omnibox::INPUT_TYPE_LENS_IMAGE) &&
      contains(omnibox::INPUT_TYPE_LENS_FILE)) {
    state_.allowed_input_types.push_back(omnibox::INPUT_TYPE_BROWSER_TAB);
  }

  state_.active_tool = omnibox::ToolMode::TOOL_MODE_UNSPECIFIED;
  // the initial model should be the first allowed model.
  state_.active_model = state_.GetDefaultModel();
  state_.image_gen_upload_active = false;

  updateDisabledState();
}

InputStateModel::InputStateModel(
    const InputStateModel& new_input_state_model,
    contextual_search::ContextualSearchSessionHandle& new_session_handle)
    : session_handle_(new_session_handle.AsWeakPtr()),
      is_off_the_record_(new_input_state_model.is_off_the_record_) {
  state_ = new_input_state_model.state_;
  rule_set_ = new_input_state_model.rule_set_;
}

InputStateModel::~InputStateModel() = default;

void InputStateModel::Initialize() {
  notifySubscribers();
}

void InputStateModel::SetPrefService(const PrefService* pref_service) {
  pref_service_ = pref_service;
  updateDisabledState();
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
    const contextual_search::ContextualSearchSessionHandle* session_handle) {
  std::vector<omnibox::InputType> input_types;
  if (!session_handle) {
    return input_types;
  }
  const auto& uploaded_files = session_handle->GetUploadedContextFileInfos();
  input_types.reserve(uploaded_files.size());
  for (const auto& file_info : uploaded_files) {
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

void InputStateModel::OnContextChanged() {
  // Update the disabled state based on the new inputs uploaded.
  updateDisabledState();

  if (state_.active_tool == omnibox::ToolMode::TOOL_MODE_IMAGE_GEN) {
    const auto current_inputs = GetCurrentInputTypes(session_handle_.get());
    if (std::find(current_inputs.begin(), current_inputs.end(),
                  omnibox::InputType::INPUT_TYPE_LENS_IMAGE) ==
        current_inputs.end()) {
      state_.image_gen_upload_active = false;
    } else {
      state_.image_gen_upload_active = true;
    }
  }

  // Notify subscribers once `state_` is updated.
  notifySubscribers();
}

void InputStateModel::updateSelectedState(ToolMode tool, ModelMode model) {
  state_.active_model = model;
  state_.image_gen_upload_active = false;

  // Set `image_gen_upload_active` to true if the active tool is
  // `TOOL_MODE_IMAGE_GEN` and an image is uploaded.
  if (tool == omnibox::ToolMode::TOOL_MODE_IMAGE_GEN) {
    const auto current_inputs = GetCurrentInputTypes(session_handle_.get());
    if (std::find(current_inputs.begin(), current_inputs.end(),
                  omnibox::InputType::INPUT_TYPE_LENS_IMAGE) !=
        current_inputs.end()) {
      state_.image_gen_upload_active = true;
    }
  }
  state_.active_tool = tool;

  // Update the disabled state based on the active model, tool, and current
  // input types.
  updateDisabledState();

  // Notify subscribers once `state_` is updated.
  notifySubscribers();
}

// Helper to check if search content sharing is enabled based on the
// user preference.
bool InputStateModel::IsSearchContentSharingEnabled() const {
  if (!pref_service_) {
    // Default behavior: if no `PrefService` default to allowed.
    return true;
  }

  // Read the pref value.
  int value = pref_service_->GetInteger(
      contextual_search::kSearchContentSharingSettings);

  // Comparison logic: must cast the enum class to an int for comparison.
  return value ==
         static_cast<int>(
             contextual_search::SearchContentSharingSettingsValue::kEnabled);
}

void InputStateModel::UpdateDisabledTools() {
  // Disable a tool if:
  // - It is currently active (to prevent re-activation).
  // - Incompatible with the active model.
  // - Incompatible with the current inputs.
  state_.disabled_tools.clear();
  state_.disabled_tools.reserve(state_.allowed_tools.size());
  const omnibox::ModelRule* active_model_rule =
      GetModelRule(rule_set_, state_.active_model);
  for (const auto& tool : state_.allowed_tools) {
    if (tool == state_.active_tool) {
      state_.disabled_tools.push_back(tool);
      continue;
    }

    bool incompatible_with_model =
            state_.active_model != omnibox::ModelMode::MODEL_MODE_UNSPECIFIED &&
            active_model_rule &&
            !active_model_rule->allow_all_tools() &&
            !IsItemAllowed(tool, active_model_rule->allowed_tools());

    const omnibox::ToolRule* tool_rule = GetToolRule(rule_set_, tool);
    bool incompatible_with_inputs =
        !tool_rule ||
        (!tool_rule->allow_all_input_types() &&
         !AreItemsAllowed(GetCurrentInputTypes(session_handle_.get()),
                          tool_rule->allowed_input_types()));

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
  state_.disabled_models.reserve(state_.allowed_models.size());

  for (const auto& model : state_.allowed_models) {
    if (model == state_.active_model) {
      continue;
    }

    const omnibox::ModelRule* model_rule = GetModelRule(rule_set_, model);

    bool incompatible_with_tool =
        state_.active_tool != omnibox::ToolMode::TOOL_MODE_UNSPECIFIED &&
        (!model_rule ||
         (!model_rule->allow_all_tools() &&
          !IsItemAllowed(state_.active_tool, model_rule->allowed_tools())));

    bool incompatible_with_inputs =
        (!model_rule ||
         (!model_rule->allow_all_input_types() &&
          !AreItemsAllowed(GetCurrentInputTypes(session_handle_.get()),
                           model_rule->allowed_input_types())));

    if (incompatible_with_tool || incompatible_with_inputs) {
      state_.disabled_models.push_back(model);
    }
  }
}

void InputStateModel::UpdateDisabledInputTypes() {
  // Disable an input type if:
  // - Enterprise policy disallows content sharing.
  // - Input type limit is reached.
  // - Total input limit is reached.
  // - Incompatible with the active model.
  // - Incompatible with the active tool.
  state_.disabled_input_types.clear();
  state_.disabled_input_types.reserve(state_.allowed_input_types.size());

  if (!IsSearchContentSharingEnabled()) {
    std::erase_if(state_.allowed_input_types, [](auto input_type) {
      return input_type == omnibox::InputType::INPUT_TYPE_LENS_IMAGE ||
             input_type == omnibox::InputType::INPUT_TYPE_LENS_FILE ||
             input_type == omnibox::InputType::INPUT_TYPE_BROWSER_TAB;
    });
  }

  const auto current_inputs = GetCurrentInputTypes(session_handle_.get());

  // Check max inputs reached.
  bool global_limit_reached =
      state_.max_total_inputs > 0 &&
      current_inputs.size() >= static_cast<size_t>(state_.max_total_inputs);

  if (global_limit_reached) {
    state_.disabled_input_types = state_.allowed_input_types;
    return;
  }

  const auto& limits = state_.max_instances;
  std::map<omnibox::InputType, int> current_input_counts;
  for (const auto& input_type : current_inputs) {
    current_input_counts[input_type]++;
  }

  const omnibox::ModelRule* active_model_rule =
      GetModelRule(rule_set_, state_.active_model);
  const omnibox::ToolRule* active_tool_rule =
      GetToolRule(rule_set_, state_.active_tool);

  for (const auto& input_type : state_.allowed_input_types) {
    bool input_limit_reached = false;
    if (auto limits_it = limits.find(input_type); limits_it != limits.end()) {
      int limit = limits_it->second;
      if (limit > 0) {
        if (auto it = current_input_counts.find(input_type);
            it != current_input_counts.end() && it->second >= limit) {
          input_limit_reached = true;
        }
      }
    }

    bool incompatible_with_model =
        state_.active_model != omnibox::ModelMode::MODEL_MODE_UNSPECIFIED &&
        active_model_rule &&
        !active_model_rule->allow_all_input_types() &&
        !IsItemAllowed(input_type, active_model_rule->allowed_input_types());

    bool incompatible_with_tool =
        state_.active_tool != omnibox::ToolMode::TOOL_MODE_UNSPECIFIED &&
        active_tool_rule && !active_tool_rule->allow_all_input_types() &&
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

std::map<std::string, std::string> InputStateModel::GetAdditionalQueryParams() {
  std::map<std::string, std::string> additional_params;
  if (state_.active_tool != omnibox::ToolMode::TOOL_MODE_UNSPECIFIED) {
    const auto tool_it =
        std::find_if(state_.tool_configs.begin(), state_.tool_configs.end(),
                     [&](const omnibox::ToolConfig& config) {
                       return config.tool() == state_.active_tool;
                     });
    if (tool_it != state_.tool_configs.end()) {
      for (const auto& param : tool_it->aim_url_params()) {
        additional_params[param.param_key()] = param.param_value();
      }
    }
  }
  if (state_.active_model != omnibox::ModelMode::MODEL_MODE_UNSPECIFIED) {
    const auto model_it =
        std::find_if(state_.model_configs.begin(), state_.model_configs.end(),
                     [&](const omnibox::ModelConfig& config) {
                       return config.model() == state_.active_model;
                     });
    if (model_it != state_.model_configs.end()) {
      for (const auto& param : model_it->aim_url_params()) {
        additional_params[param.param_key()] = param.param_value();
      }
    }
  } else {
    // If no model is selected, add a default param to indicate that the query
    // is an AIM query.
    additional_params["udm"] = "50";
  }
  return additional_params;
}

}  // namespace contextual_search
